/**
 * @file rtdb.c
 * @brief RTDB Implementation.
 */
#include "rtdb.h"

K_MUTEX_DEFINE(rtdb_mutex);

// Internal state storage
static rtdb_state_t rtdb_current_state = {
    .wave_type = WAVE_SQUARE,
    .amplitude_v = 0.0f,
    .frequency_hz = 10,
    .output_active = false
};

// Internal statistics storage
static sys_stats_t global_stats = {0};

void rtdb_update_metric(int metric_id, uint32_t duration_cycles) {
    // Convert cycles to microseconds immediately for storage
    uint32_t us = k_cyc_to_us_floor32(duration_cycles);
    
    // Simple atomic 32-bit writes (No mutex needed for stats to avoid priority inversion)
    switch(metric_id) {
        case 0: // SigGen (Critical)
            if(us > global_stats.wcet_siggen_us) global_stats.wcet_siggen_us = us;
            break;
        case 1: // Input
            if(us > global_stats.resp_input_us) global_stats.resp_input_us = us;
            break;
        case 2: // Command
            if(us > global_stats.resp_cmd_us) global_stats.resp_cmd_us = us;
            break;
        case 3: // Output
            if(us > global_stats.resp_output_us) global_stats.resp_output_us = us; 
            break;
    }
}

sys_stats_t rtdb_get_stats_copy(void) {
    return global_stats;
}

// --- Thread-Safe Setters ---

void rtdb_set_amplitude(float amp_v) {
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    if (amp_v < 0.0f) amp_v = 0.0f;
    if (amp_v > 2.5f) amp_v = 2.5f;
    rtdb_current_state.amplitude_v = amp_v;
    k_mutex_unlock(&rtdb_mutex);
}

void rtdb_set_frequency(int freq_hz) {
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    if (freq_hz < 1) freq_hz = 1; 
    if (freq_hz > 100) freq_hz = 100; 
    rtdb_current_state.frequency_hz = freq_hz;
    k_mutex_unlock(&rtdb_mutex);
}

void rtdb_set_output_active(bool active) {
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    rtdb_current_state.output_active = active;
    k_mutex_unlock(&rtdb_mutex);
}

void rtdb_set_wave_type(wave_type_t new_type) {
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    rtdb_current_state.wave_type = new_type;
    k_mutex_unlock(&rtdb_mutex);
}

rtdb_state_t rtdb_get_state_copy(void) {
    rtdb_state_t copy;
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    copy = rtdb_current_state;
    k_mutex_unlock(&rtdb_mutex);
    return copy;
}