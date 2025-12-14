#ifndef RTDB_H
#define RTDB_H

#include <zephyr/kernel.h>
#include <stdbool.h>

/**
 * @file rtdb.h
 * @brief Real-Time Database (RTDB) Module.
 * * This module acts as a thread-safe shared memory region.
 * It uses Mutexes to protect read/write operations on the system state,
 * preventing Race Conditions between the High-Priority SigGen thread
 * and the Lower-Priority Command/Input threads.
 */

/* ============================================================================
 * TIPOS DE DADOS
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

/* ============================================================================
 * API PÚBLICA (Thread-Safe)
 * ============================================================================
 */

/**
 * @brief Initializes the Real-Time Database.
 * Must be called before any thread tries to access the state.
 * Initializes the internal Mutex.
 */
void rtdb_init(void);

/**
 * @brief Sets the signal frequency.
 * * Thread-safe setter protected by k_mutex.
 * * @param freq Frequency in Hz (Range: 10 to 100).
 */
void rtdb_set_frequency(int freq);

/**
 * @brief Sets the signal amplitude.
 * * Thread-safe setter protected by k_mutex.
 * * @param amp Amplitude in Volts (Range: 0.0 to 2.5).
 */
void rtdb_set_amplitude(float amp);

/**
 * @brief Sets the waveform type.
 * * @param type One of WAVE_SQUARE, WAVE_TRIANGLE, WAVE_SINE.
 */
void rtdb_set_wave_type(wave_type_t type);

/**
 * @brief Toggles the main output.
 * * @param active true to enable PWM output, false to disable (0V).
 */
void rtdb_set_output_active(bool active);

/**
 * @brief Gets a consistent copy of the full system state.
 * * This function locks the mutex, copies the struct, and unlocks.
 * It ensures that the consumer (e.g., SigGen) never reads a half-updated state.
 * * @return A copy of the current rtdb_state_t.
 */
rtdb_state_t rtdb_get_state_copy(void);

#endif // RTDB_H