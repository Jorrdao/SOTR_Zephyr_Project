#ifndef GANTT_LOGGER_H
#define GANTT_LOGGER_H

#include <zephyr/kernel.h>
#include <stdbool.h>

/* ============================================================================
 * LOCK-FREE GANTT CHART LOGGER
 * ============================================================================
 * Este módulo implementa logging de eventos de thread SEM MUTEXES.
 * 
 * PRINCÍPIO DE FUNCIONAMENTO:
 * - Ring buffer atómico (escrita lock-free)
 * - Apenas timestamps e IDs (operação < 10 ciclos CPU)
 * - Thread de baixa prioridade faz flush para ficheiro/UART
 * - Threads RT nunca bloqueiam
 * ============================================================================
 */

/* --- CONFIGURAÇÃO --- */
#define GANTT_LOG_BUFFER_SIZE 512   // Deve ser potência de 2
#define GANTT_MAX_THREAD_NAME 16

/* --- TIPOS DE EVENTOS --- */
typedef enum {
    GANTT_EVENT_START = 0,
    GANTT_EVENT_END   = 1
} gantt_event_type_t;

/* --- ESTRUTURA DE EVENTO (8 bytes, cache-line friendly) --- */
typedef struct {
    uint32_t timestamp_us;           // Timestamp em microsegundos
    uint8_t  thread_id;              // ID da thread (0-255)
    uint8_t  event_type;             // START ou END
    uint16_t reserved;               // Padding para alinhamento
} gantt_event_t;

/* --- API PÚBLICA --- */

/**
 * @brief Inicializa o sistema de logging Gantt
 * @return 0 em sucesso, negativo em erro
 */
int gantt_logger_init(void);

/**
 * @brief Regista uma thread para logging
 * @param thread_name Nome da thread (máx. 15 chars)
 * @return ID atribuído à thread (usar em gantt_log_event)
 */
uint8_t gantt_register_thread(const char *thread_name);

/**
 * @brief Regista evento de thread (LOCK-FREE, não bloqueia!)
 * @param thread_id ID obtido em gantt_register_thread()
 * @param event_type GANTT_EVENT_START ou GANTT_EVENT_END
 * 
 * NOTA CRÍTICA: Esta função NUNCA bloqueia. Se o buffer estiver cheio,
 * o evento é descartado silenciosamente (melhor perder um evento
 * do que violar garantias de tempo real).
 */
void gantt_log_event(uint8_t thread_id, gantt_event_type_t event_type);

/**
 * @brief Inicia thread de background que faz dump dos logs
 * Deve ser chamada APÓS todas as threads RT estarem criadas.
 */

void gantt_set_enabled(bool enable);


void gantt_start_logger_thread(void);

/* --- MACROS DE CONVENIÊNCIA --- */

/**
 * Uso típico dentro de uma thread RT:
 * 
 * void my_rt_thread_entry(void) {
 *     uint8_t my_id = gantt_register_thread("MySigGen");
 *     
 *     while(1) {
 *         GANTT_LOG_START(my_id);
 *         // ... trabalho da thread ...
 *         GANTT_LOG_END(my_id);
 *         k_msleep(period);
 *     }
 * }
 */
#define GANTT_LOG_START(thread_id) gantt_log_event(thread_id, GANTT_EVENT_START)
#define GANTT_LOG_END(thread_id)   gantt_log_event(thread_id, GANTT_EVENT_END)

#endif // GANTT_LOGGER_H
