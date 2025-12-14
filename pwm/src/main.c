/**
 * @file main.c
 * @brief System Entry Point and Orchestrator.
 * * This module is responsible for the system initialization sequence:
 * 1. Initializes the Real-Time Database (RTDB) [Implicit].
 * 2. Initializes the Gantt Logger subsystem.
 * 3. Spawns all worker threads in order of criticality.
 * * @note Architecture: The system follows a prioritized preemptive scheduling model
 * compliant with Rate Monotonic Scheduling (RMS) principles.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

// Incluir TODOS os headers necessários
#include "rtdb.h"
#include "output_thread.h"
#include "input_thread.h"  
#include "siggen_thread.h"  
#include "command_thread.h" 
#include "gantt_logger.h"

/* --- THREAD STACK DEFINITIONS --- */

/** @brief Stack for Signal Generator (High Prio). Requires FPU context. */
K_THREAD_STACK_DEFINE(siggen_thread_stack, SIGGEN_THREAD_STACK_SIZE);
static struct k_thread siggen_thread_data;

/** @brief Stack for Input Processing. */
K_THREAD_STACK_DEFINE(input_thread_stack, INPUT_THREAD_STACK_SIZE);
static struct k_thread input_thread_data;

/** @brief Stack for Command Parser. Needs buffer space for string manipulation. */
K_THREAD_STACK_DEFINE(command_thread_stack, COMMAND_THREAD_STACK_SIZE);
static struct k_thread command_thread_data;

/** @brief Stack for Output/LEDs. Minimal requirements. */
K_THREAD_STACK_DEFINE(output_thread_stack, OUTPUT_THREAD_STACK_SIZE);
static struct k_thread output_thread_data;

/**
 * @brief Main Application Entry Point.
 * * Configures the system and launches the Real-Time Operating System kernel.
 * Never returns.
 */
void main(void) {
    printk("\n--- AWG System: Real-Time Orchestration ---\n");
    // TODO: Chame a função de inicialização do RTDB aqui, se for externa (e.g., rtdb_init()).

    // Initialize Telemetry
    if (gantt_logger_init() != 0) {
        printk("Failed to init Gantt Logger!\n");
    }

    // 1. T_SigGen: Geração de Sinal (Prioridade ALTA: Prio 2)
    /** * @brief Launches the Critical Signal Generator Thread.
     * Priority: 2 (Highest). Period: 100us (Hard Real-Time).
     */
    k_thread_create(&siggen_thread_data, siggen_thread_stack,
                    K_THREAD_STACK_SIZEOF(siggen_thread_stack),
                    siggen_thread_entry, 
                    NULL, NULL, NULL,
                    SIGGEN_THREAD_PRIORITY, 0, K_NO_WAIT);
    printk("T_SigGen Launched (Prio: %d)\n", SIGGEN_THREAD_PRIORITY);

    
    // 2. T_InputUpdate: Processamento de Botões (Prioridade MÉDIA/ALTA: Prio 5)
    /** * @brief Launches the Input Handling Thread.
     * Priority: 5. Event-driven (Interrupts).
     */
    k_thread_create(&input_thread_data, input_thread_stack,
                    K_THREAD_STACK_SIZEOF(input_thread_stack),
                    input_thread_entry,
                    NULL, NULL, NULL,
                    INPUT_THREAD_PRIORITY, 0, K_NO_WAIT);
    printk("T_InputUpdate Launched (Prio: %d)\n", INPUT_THREAD_PRIORITY);


    // 3. T_CommandParser: Processamento UART (Prioridade MÉDIA/BAIXA: Prio 7)
    /** * @brief Launches the UART Command Parser Thread.
     * Priority: 7. Handles heavy string parsing without blocking SigGen.
     */
    k_thread_create(&command_thread_data, command_thread_stack,
                    K_THREAD_STACK_SIZEOF(command_thread_stack),
                    command_thread_entry,
                    NULL, NULL, NULL,
                    COMMAND_THREAD_PRIORITY, 0, K_NO_WAIT);
    printk("T_CommandParser Launched (Prio: %d)\n", COMMAND_THREAD_PRIORITY);


    // 4. T_OutputUpdate: Controlo de LEDs (Prioridade BAIXA: Prio 10)
    /** * @brief Launches the LED Update Thread.
     * Priority: 10 (Lowest). Visual feedback is non-critical.
     */
    k_thread_create(&output_thread_data, output_thread_stack,
                    K_THREAD_STACK_SIZEOF(output_thread_stack),
                    output_thread_entry,
                    NULL, NULL, NULL,
                    OUTPUT_THREAD_PRIORITY, 0, K_NO_WAIT);
    printk("T_OutputUpdate Launched (Prio: %d)\n", OUTPUT_THREAD_PRIORITY);


    // Start the background logger worker
    gantt_start_logger_thread();

    printk("RTOS system initialization complete.\n");
}