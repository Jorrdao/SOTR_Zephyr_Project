#ifndef GANTT_LOGGER_H
#define GANTT_LOGGER_H

#include <zephyr/kernel.h>
#include <stdbool.h>

/**
 * @file gantt_logger.h
 * @brief Low-Overhead Lock-Free Logger for Gantt Chart generation.
 * * This module records thread execution events (START/END) into a 
 * circular buffer (Ring Buffer) using atomic operations.
 * * @note This implementation is "Lock-Free" to ensure that High-Priority
 * Real-Time threads (like SigGen) are NEVER blocked by logging operations.
 */

/* --- CONFIGURATION --- */
#define GANTT_LOG_BUFFER_SIZE 512   /**< Buffer size (Power of 2 for optimization) */
#define GANTT_MAX_THREAD_NAME 16    /**< Max chars for thread name */

/**
 * @brief Event Types for Gantt Chart.
 */
typedef enum {
    GANTT_EVENT_START = 0,  /**< Thread started processing (CPU busy) */
    GANTT_EVENT_END   = 1   /**< Thread finished processing (Yield/Sleep) */
} gantt_event_type_t;

/**
 * @brief Event Structure (Optimized for memory alignment).
 * Size: 8 bytes per event.
 */
typedef struct {
    uint32_t timestamp_us;  /**< Timestamp in microseconds */
    uint8_t  thread_id;     /**< Unique Thread ID */
    uint8_t  event_type;    /**< START or END */
    uint16_t reserved;      /**< Padding */
} gantt_event_t;

/* ============================================================================
 * API FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Initializes the Logger subsystem.
 * Resets ring buffer indices and registers.
 * @return 0 on success.
 */
int gantt_logger_init(void);

/**
 * @brief Registers a thread for logging.
 * * Assigns a unique ID to the thread name for efficient logging.
 * * @param thread_name Human-readable name (e.g., "T_SigGen").
 * @return Assigned Thread ID (uint8_t).
 */
uint8_t gantt_register_thread(const char *thread_name);

/**
 * @brief Logs a thread event (Critical Path).
 * * This function is LOCK-FREE and generic. It uses atomic_inc to
 * reserve space in the ring buffer.
 * If the buffer is full, the event is silently dropped to preserve system timing.
 * * @param thread_id ID obtained from gantt_register_thread().
 * @param event_type GANTT_EVENT_START or GANTT_EVENT_END.
 */
void gantt_log_event(uint8_t thread_id, gantt_event_type_t event_type);

/**
 * @brief Controls the Logging Output.
 * * Used to enable/disable the stream of CSV data to UART.
 * * @param enable true to start sending data, false to silence UART.
 */
void gantt_set_enabled(bool enable);

/**
 * @brief Starts the background worker thread.
 * * This thread runs at Low Priority to offload data from the ring buffer
 * to the UART interface without disturbing Real-Time tasks.
 */
void gantt_start_logger_thread(void);

/* --- MACROS --- */
#define GANTT_LOG_START(id) gantt_log_event(id, GANTT_EVENT_START)
#define GANTT_LOG_END(id)   gantt_log_event(id, GANTT_EVENT_END)

#endif // GANTT_LOGGER_H