#ifndef SIGGEN_THREAD_H
#define SIGGEN_THREAD_H

#include <zephyr/kernel.h>

/**
 * @file siggen_thread.h
 * @brief Signal Generator Thread Interface.
 * * This module is the "Hard Real-Time" core of the project.
 * It is responsible for calculating waveform samples and updating the PWM duty cycle
 * within a strict deadline (100us).
 */

/* --- CONFIGURATION --- */

/** @brief Stack size. Needs to be generous for floating-point operations (FPU). */
#define SIGGEN_THREAD_STACK_SIZE 1024 

/** * @brief Thread Priority (2).
 * VERY HIGH PRIORITY. In Zephyr, lower number = higher priority.
 * This ensures the signal generator preempts all other tasks (Input, UART, LEDs).
 */
#define SIGGEN_THREAD_PRIORITY   2 

/* --- API --- */

/**
 * @brief Signal Generator Thread Entry Point.
 * * Contains the main loop that:
 * 1. Reads configuration from RTDB.
 * 2. Feeds the Watchdog Timer.
 * 3. Manages the hardware timer for precise sample output.
 */
void siggen_thread_entry(void *p1, void *p2, void *p3);

/**
 * @brief Retrieves the measured Worst-Case Execution Time (WCET).
 * * Accesses the internal profiling variable safely.
 * @return The maximum execution time measured so far (in microseconds).
 */
uint32_t get_siggen_wcet_us(void);

#endif // SIGGEN_THREAD_H