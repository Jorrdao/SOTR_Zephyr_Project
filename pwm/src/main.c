// main.c (Completo)

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

// Incluir TODOS os headers necessários
#include "rtdb.h"
#include "output_thread.h"
#include "input_thread.h"  
#include "siggen_thread.h"  
#include "command_thread.h" 
#include "gantt_logger.h"

// --- Definições para a thread de SigGen (Prioridade ALTA) ---
K_THREAD_STACK_DEFINE(siggen_thread_stack, SIGGEN_THREAD_STACK_SIZE);
static struct k_thread siggen_thread_data;

// --- Definições para a thread de Input (Prioridade MÉDIA/ALTA) ---
K_THREAD_STACK_DEFINE(input_thread_stack, INPUT_THREAD_STACK_SIZE);
static struct k_thread input_thread_data;

// --- Definições para a thread de CommandParser (Prioridade MÉDIA/BAIXA) ---
K_THREAD_STACK_DEFINE(command_thread_stack, COMMAND_THREAD_STACK_SIZE);
static struct k_thread command_thread_data;

// --- Definições para a thread de Output (Prioridade BAIXA) ---
K_THREAD_STACK_DEFINE(output_thread_stack, OUTPUT_THREAD_STACK_SIZE);
static struct k_thread output_thread_data;


void main(void) {
    printk("\n--- AWG System: Real-Time Orchestration ---\n");
    // TODO: Chame a função de inicialização do RTDB aqui, se for externa (e.g., rtdb_init()).
    if(gantt_logger_init() != 0) {
        printk("Gantt Logger initialization failed!\n");
        return;
    } 
    // 1. T_SigGen: Geração de Sinal (Prioridade ALTA: Prio 2)
    k_thread_create(&siggen_thread_data, siggen_thread_stack,
                    K_THREAD_STACK_SIZEOF(siggen_thread_stack),
                    siggen_thread_entry, // <-- Função de entrada da T_SigGen
                    NULL, NULL, NULL,
                    SIGGEN_THREAD_PRIORITY, 0, K_NO_WAIT);
    printk("T_SigGen Launched (Prio: %d)\n", SIGGEN_THREAD_PRIORITY);

    
    // 2. T_InputUpdate: Processamento de Botões (Prioridade MÉDIA/ALTA: Prio 5)
    k_thread_create(&input_thread_data, input_thread_stack,
                    K_THREAD_STACK_SIZEOF(input_thread_stack),
                    input_thread_entry,
                    NULL, NULL, NULL,
                    INPUT_THREAD_PRIORITY, 0, K_NO_WAIT);
    printk("T_InputUpdate Launched (Prio: %d)\n", INPUT_THREAD_PRIORITY);


    // 3. T_CommandParser: Processamento UART (Prioridade MÉDIA/BAIXA: Prio 7)
    k_thread_create(&command_thread_data, command_thread_stack,
                    K_THREAD_STACK_SIZEOF(command_thread_stack),
                    command_thread_entry,
                    NULL, NULL, NULL,
                    COMMAND_THREAD_PRIORITY, 0, K_NO_WAIT);
    printk("T_CommandParser Launched (Prio: %d)\n", COMMAND_THREAD_PRIORITY);


    // 4. T_OutputUpdate: Controlo de LEDs (Prioridade BAIXA: Prio 10)
    k_thread_create(&output_thread_data, output_thread_stack,
                    K_THREAD_STACK_SIZEOF(output_thread_stack),
                    output_thread_entry,
                    NULL, NULL, NULL,
                    OUTPUT_THREAD_PRIORITY, 0, K_NO_WAIT);
    printk("T_OutputUpdate Launched (Prio: %d)\n", OUTPUT_THREAD_PRIORITY);
    gantt_start_logger_thread();

    printk("RTOS system initialization complete.\n");
}