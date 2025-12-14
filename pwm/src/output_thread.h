#ifndef OUTPUT_THREAD_H
#define OUTPUT_THREAD_H

/**
 * @file output_thread.h
 * @brief Visual Feedback (LED) Interface.
 * * Updates the physical LEDs on the board to reflect the internal state
 * of the system (Waveform type, Output ON/OFF).
 */

/* --- CONFIGURATION --- */

/** @brief Stack size. Minimal requirements. */
#define OUTPUT_THREAD_STACK_SIZE 2048 

/** * @brief Thread Priority (10).
 * Low Priority. This is a "Soft Real-Time" task. If it lags by 50ms, 
 * the human eye won't notice, so it yields CPU to everything else.
 */
#define OUTPUT_THREAD_PRIORITY   10 

/* --- API --- */

/**
 * @brief Output Thread Entry Point.
 * * Periodically polls the RTDB state and updates GPIO pins.
 * Rate limited to avoid consuming unnecessary CPU cycles.
 */
void output_thread_entry(void *p1, void *p2, void *p3);

#endif // OUTPUT_THREAD_H