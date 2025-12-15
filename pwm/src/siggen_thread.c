/**
 * @file siggen_thread.c
 * @brief Signal Generator Thread (Hard Real-Time).
 * * Features:
 * - Waveform Synthesis (Sine/Triangle/Square) via PWM.
 * - Software Profiling to validate RMS assumptions.
 */

#include "siggen_thread.h"
#include "rtdb.h"
#include "gantt_logger.h"
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/printk.h>

/* --- HARDWARE CONFIG --- */
#define PWM_NODE DT_ALIAS(signal_pwm)
#define PWM_CARRIER_NS 20000 
#define BOARD_VDD_VOLTAGE 3.3f 

/* --- LUT (100 points) --- */
static const float sine_lut[100] = {
    0.500f, 0.531f, 0.563f, 0.594f, 0.625f, 0.655f, 0.684f, 0.713f, 0.740f, 0.766f,
    0.790f, 0.813f, 0.835f, 0.855f, 0.873f, 0.890f, 0.905f, 0.918f, 0.930f, 0.940f,
    0.949f, 0.957f, 0.963f, 0.969f, 0.973f, 0.976f, 0.978f, 0.979f, 0.979f, 0.978f,
    0.976f, 0.973f, 0.969f, 0.963f, 0.957f, 0.949f, 0.940f, 0.930f, 0.918f, 0.905f,
    0.890f, 0.873f, 0.855f, 0.835f, 0.813f, 0.790f, 0.766f, 0.740f, 0.713f, 0.684f,
    0.655f, 0.625f, 0.594f, 0.563f, 0.531f, 0.500f, 0.469f, 0.437f, 0.406f, 0.375f,
    0.345f, 0.316f, 0.287f, 0.260f, 0.234f, 0.210f, 0.187f, 0.165f, 0.145f, 0.127f,
    0.110f, 0.095f, 0.082f, 0.070f, 0.060f, 0.051f, 0.043f, 0.037f, 0.031f, 0.027f,
    0.024f, 0.022f, 0.021f, 0.021f, 0.021f, 0.022f, 0.024f, 0.027f, 0.031f, 0.037f,
    0.043f, 0.051f, 0.060f, 0.070f, 0.082f, 0.095f, 0.110f, 0.127f, 0.145f, 0.165f,
    0.187f, 0.210f, 0.234f, 0.260f, 0.287f, 0.316f, 0.345f, 0.375f, 0.406f, 0.437f
};

static const struct pwm_dt_spec pwm_dev = PWM_DT_SPEC_GET(PWM_NODE);
static struct k_timer sample_timer;
static volatile int sample_index = 0;
static volatile wave_type_t current_wave_type = WAVE_SQUARE;
static volatile float current_amplitude_v = 0.0f;
static volatile bool is_output_active = false;

/* --- TIMER HANDLER (CRITICAL SECTION) --- */
void sample_timer_handler(struct k_timer *dummy) {
    // 1. START PROFILING
    uint32_t start = k_cycle_get_32();

    if (!is_output_active) return;
    
    float shape_value = 0.0f; 
    switch (current_wave_type) {
        case WAVE_SINE: shape_value = sine_lut[sample_index]; break;
        case WAVE_TRIANGLE: 
            if (sample_index < 50) shape_value = (float)sample_index / 50.0f;
            else shape_value = (float)(100 - sample_index) / 50.0f;
            break;
        default: 
            if (sample_index < 50) shape_value = 1.0f; else shape_value = 0.0f;
            break;
    }
    
    float target_v = current_amplitude_v;
    if (target_v > 2.5f) target_v = 2.5f; 
    if (target_v < 0.0f) target_v = 0.0f;
    float scale_factor = target_v / BOARD_VDD_VOLTAGE;
    uint32_t pulse_ns = (uint32_t)(shape_value * scale_factor * PWM_CARRIER_NS);
    
    pwm_set_dt(&pwm_dev, PWM_CARRIER_NS, pulse_ns);
    
    sample_index++;
    if (sample_index >= 100) sample_index = 0;

    // 2. END PROFILING & REPORT
    uint32_t end = k_cycle_get_32();
    rtdb_update_metric(0, end - start); // ID 0 = SigGen WCET
}

void siggen_thread_entry(void *p1, void *p2, void *p3) {
    uint8_t gantt_id = gantt_register_thread("T_SigGen");

    if (!pwm_is_ready_dt(&pwm_dev)) {
        printk("SIGGEN FATAL: PWM device not ready!\n");
        return;
    }

    k_timer_init(&sample_timer, sample_timer_handler, NULL);
    
    int last_freq = -1;
    bool last_active = false;

    while (1) {
        GANTT_LOG_START(gantt_id);
        
        // Critical: Minimize time here
        rtdb_state_t state = rtdb_get_state_copy();
        current_wave_type = state.wave_type;
        current_amplitude_v = state.amplitude_v;
        is_output_active = state.output_active;

        if (state.output_active) {
            if (!last_active || state.frequency_hz != last_freq) {
                int safe_freq = state.frequency_hz;
                if (safe_freq < 1) safe_freq = 1;
                uint32_t sample_period_us = 1000000 / (safe_freq * 100);
                k_timer_start(&sample_timer, K_USEC(sample_period_us), K_USEC(sample_period_us));
            }
        } else {
            if (last_active) {
                k_timer_stop(&sample_timer);
                pwm_set_dt(&pwm_dev, PWM_CARRIER_NS, 0); 
            }
        }
        last_freq = state.frequency_hz;
        last_active = state.output_active;

        GANTT_LOG_END(gantt_id);
        k_msleep(100);
    }
}