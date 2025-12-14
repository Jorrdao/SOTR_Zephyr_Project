#ifndef INPUT_THREAD_H
#define INPUT_THREAD_H

#include <zephyr/kernel.h>

/**
 * @file input_thread.h
 * @brief Human-Machine Interface (HMI) Handling Module.
 * * This module manages the 4 physical buttons on the nRF52840-DK.
 * It uses interrupts and semaphores to react to user input with low latency.
 */

/* --- CONFIGURAÇÃO DA THREAD --- */

/** @brief Stack size in bytes. Sufficient for basic GPIO and RTDB calls. */
#define INPUT_THREAD_STACK_SIZE 1024 

/** @brief Priority level (5). Higher than UART (7) and Output (10), lower than SigGen (2). */
#define INPUT_THREAD_PRIORITY   5 

/* --- API PÚBLICA --- */

/**
 * @brief Entry point for the Input Processing Thread.
 * * This thread sleeps on a semaphore waiting for GPIO interrupts.
 * When a button is pressed, it wakes up, debounces the signal, 
 * updates the RTDB state, and logs the event to the Gantt chart.
 */
void input_thread_entry(void *p1, void *p2, void *p3);

#endif // INPUT_THREAD_H