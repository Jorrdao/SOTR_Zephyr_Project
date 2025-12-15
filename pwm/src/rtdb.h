#ifndef RTDB_H
#define RTDB_H

#include <zephyr/kernel.h>
#include <stdbool.h>

/**
 * @file rtdb.h
 * @brief Real-Time Database (RTDB) Module.
 * * This module acts as the central Shared Memory region for the system.
 * It implements Thread-Safe access using Mutexes to prevent Race Conditions
 * and ensure data consistency between high-priority (SigGen) and low-priority tasks.
 */

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================
 */

/**
 * @brief Supported Waveform Types.
 */
typedef enum {
    WAVE_SQUARE = 0,    /**< Square wave (Digital Pulse) */
    WAVE_TRIANGLE = 1,  /**< Triangle wave (Linear Ramp) */
    WAVE_SINE = 2       /**< Sine wave (Look-Up Table based) */
} wave_type_t;

/**
 * @brief System State Structure.
 * Holds the current configuration of the waveform generator.
 */
typedef struct {
    int frequency_hz;      /**< Frequency in Hertz (10-100Hz) */
    float amplitude_v;     /**< Amplitude in Volts (0.0-2.5V) */
    wave_type_t wave_type; /**< Current waveform type */
    bool output_active;    /**< Master Output Switch (ON/OFF) */
} rtdb_state_t;

/**
 * @brief Real-Time System Statistics (Profiling).
 * Stores the maximum measured execution times (High-Water Mark).
 */
typedef struct {
    uint32_t wcet_siggen_us;  /**< Worst-Case Execution Time for Hard RT Task (SigGen) */
    uint32_t resp_input_us;   /**< Response Time for Input Task (Exec + Blocking) */
    uint32_t resp_cmd_us;     /**< Response Time for Command Task (Exec + Blocking) */
    uint32_t resp_output_us;
} sys_stats_t;

/* ============================================================================
 * PUBLIC API (Thread-Safe)
 * ============================================================================
 */

/**
 * @brief Initializes the Real-Time Database.
 * Must be called before any thread tries to access the state.
 */
void rtdb_init(void);

/**
 * @brief Updates a specific timing metric.
 * * Used by worker threads to report their execution duration.
 * Keeps the maximum value observed (Peak Hold).
 * * @param metric_id 0=SigGen(WCET), 1=Input(Response), 2=Command(Response).
 * @param duration_cycles Duration in CPU cycles.
 */
void rtdb_update_metric(int metric_id, uint32_t duration_cycles);

/**
 * @brief Gets a copy of the system statistics.
 * * @return A sys_stats_t struct containing the max times in microseconds.
 */
sys_stats_t rtdb_get_stats_copy(void);

// --- Setters ---
void rtdb_set_frequency(int freq);
void rtdb_set_amplitude(float amp);
void rtdb_set_wave_type(wave_type_t type);
void rtdb_set_output_active(bool active);

/**
 * @brief Gets a consistent copy of the full system state.
 * * Locks the mutex to ensure atomic read.
 * * @return A copy of rtdb_state_t.
 */
rtdb_state_t rtdb_get_state_copy(void);

#endif // RTDB_H