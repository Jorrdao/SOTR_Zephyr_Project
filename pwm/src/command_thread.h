#ifndef COMMAND_THREAD_H
#define COMMAND_THREAD_H

#include <zephyr/kernel.h>

/**
 * @file command_thread.h
 * @brief UART Command Processing Interface.
 * * Handles protocol parsing, checksum validation, and execution of commands
 * received from the host PC.
 */

/* --- CONFIGURATION --- */

/** @brief Stack size. Needs space for string buffers (rx_buf). */
#define COMMAND_THREAD_STACK_SIZE 1024 

/** * @brief Thread Priority (7).
 * Medium/Low Priority. Higher than visual output, but lower than Input and SigGen.
 * Allows parsing complex strings (sprintf/atoi) without causing jitter in the signal output.
 */
#define COMMAND_THREAD_PRIORITY   7 

/* --- API --- */

/**
 * @brief Command Parser Thread Entry Point.
 * * Implements a polling loop on the UART interface to:
 * 1. Assemble incoming characters into packets.
 * 2. Validate Checksums (Data Integrity).
 * 3. Update RTDB based on commands (SF, SA, SO).
 * 4. Control the Gantt Logger state (GL).
 */
void command_thread_entry(void *p1, void *p2, void *p3);

#endif // COMMAND_THREAD_H