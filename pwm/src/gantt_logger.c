#include "gantt_logger.h"
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/atomic.h>
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * IMPLEMENTAÇÃO DO LOGGER LOCK-FREE
 * ============================================================================
 */

/* --- ESTRUTURA DO RING BUFFER --- */
static struct {
    gantt_event_t events[GANTT_LOG_BUFFER_SIZE];
    atomic_t write_idx;  // Índice de escrita (atómico!)
    atomic_t read_idx;   // Índice de leitura (atómico!)
    atomic_t dropped_events; // Contador de eventos perdidos
} gantt_ring_buffer;

static bool logger_enabled = false; // Começa desligado (Silencioso)

/* --- REGISTO DE THREADS --- */
typedef struct {
    char name[GANTT_MAX_THREAD_NAME];
    bool in_use;
} thread_info_t;

static thread_info_t registered_threads[256];
static atomic_t next_thread_id = ATOMIC_INIT(0);

/* --- UART PARA OUTPUT --- */
static const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

/* --- THREAD DE LOGGING --- */
#define LOGGER_THREAD_STACK_SIZE 2048
#define LOGGER_THREAD_PRIORITY   15  // Prioridade MUITO BAIXA

K_THREAD_STACK_DEFINE(logger_thread_stack, LOGGER_THREAD_STACK_SIZE);
static struct k_thread logger_thread_data;

/* ============================================================================
 * FUNÇÃO DE INICIALIZAÇÃO
 * ============================================================================
 */
int gantt_logger_init(void) {
    // Inicializar índices do ring buffer
    atomic_set(&gantt_ring_buffer.write_idx, 0);
    atomic_set(&gantt_ring_buffer.read_idx, 0);
    atomic_set(&gantt_ring_buffer.dropped_events, 0);
    
    // Limpar registo de threads
    memset(registered_threads, 0, sizeof(registered_threads));
    
    if (!device_is_ready(uart_dev)) {
        printk("GANTT LOGGER ERROR: UART not ready\n");
        return -1;
    }
    
    printk("GANTT LOGGER: Initialized (Buffer size: %d events)\n", 
           GANTT_LOG_BUFFER_SIZE);
    
    return 0;
}

/* ============================================================================
 * REGISTO DE THREAD
 * ============================================================================
 */
uint8_t gantt_register_thread(const char *thread_name) {
    uint8_t id = (uint8_t)atomic_inc(&next_thread_id);
    
    if (id >= 256) {
        printk("GANTT ERROR: Too many threads!\n");
        return 255;
    }
    
    strncpy(registered_threads[id].name, thread_name, GANTT_MAX_THREAD_NAME - 1);
    registered_threads[id].name[GANTT_MAX_THREAD_NAME - 1] = '\0';
    registered_threads[id].in_use = true;
    
    //printk("GANTT: Thread '%s' registered with ID %d\n", thread_name, id);
    
    return id;
}

/* ============================================================================
 * LOGGING DE EVENTO (LOCK-FREE!)
 * ============================================================================
 * CRÍTICO: Esta função NUNCA bloqueia!
 */
void gantt_log_event(uint8_t thread_id, gantt_event_type_t event_type) {
    // 1. Obter timestamp (operação rápida, ~50 ciclos)
    uint32_t timestamp_us = k_cyc_to_us_floor32(k_cycle_get_32());
    
    // 2. Obter índice de escrita (atómico, sem lock!)
    uint32_t write_pos = atomic_inc(&gantt_ring_buffer.write_idx);
    write_pos = write_pos % GANTT_LOG_BUFFER_SIZE;
    
    // 3. Verificar se buffer está cheio (sem blocking!)
    uint32_t read_pos = atomic_get(&gantt_ring_buffer.read_idx);
    uint32_t next_write = (write_pos + 1) % GANTT_LOG_BUFFER_SIZE;
    
    if (next_write == read_pos) {
        // Buffer cheio! Descartar evento (melhor que bloquear!)
        atomic_inc(&gantt_ring_buffer.dropped_events);
        return;
    }
    
    // 4. Escrever evento (operação < 10 ciclos)
    gantt_ring_buffer.events[write_pos].timestamp_us = timestamp_us;
    gantt_ring_buffer.events[write_pos].thread_id = thread_id;
    gantt_ring_buffer.events[write_pos].event_type = (uint8_t)event_type;
    gantt_ring_buffer.events[write_pos].reserved = 0;
    
    // NOTA: Sem barriers de memória necessárias porque Zephyr garante
    // ordering em atomic operations no ARM Cortex-M
}

/* ============================================================================
 * THREAD DE LOGGING (Background, baixa prioridade)
 * ============================================================================
 */
static void send_uart_line(const char *line) {
    for (size_t i = 0; i < strlen(line); i++) {
        uart_poll_out(uart_dev, line[i]);
    }
}

static void logger_thread_entry(void *p1, void *p2, void *p3) {
    char csv_line[128];
    
    // Opcional: Enviar cabeçalho apenas quando liga, mas aqui garante que temos sempre um
    // send_uart_line("timestamp_us,thread_id,thread_name,event_type\r\n");
    
    while (1) {
        // --- MODO SILENCIOSO ---
        if (!logger_enabled) {
            // Esvaziar o buffer silenciosamente (discard)
            // Isto garante que quando ligares, vês dados novos e não velhos
            atomic_val_t write_now = atomic_get(&gantt_ring_buffer.write_idx);
            atomic_set(&gantt_ring_buffer.read_idx, write_now);
            
            k_msleep(100); // Dorme e verifica de novo daqui a pouco
            continue; 
        }

        // --- MODO ATIVO (Igual ao que tinhas) ---
        uint32_t read_pos = atomic_get(&gantt_ring_buffer.read_idx);
        uint32_t write_pos = atomic_get(&gantt_ring_buffer.write_idx);
        
        if (read_pos != write_pos) {
            gantt_event_t evt = gantt_ring_buffer.events[read_pos % GANTT_LOG_BUFFER_SIZE];
            
            const char *thread_name = "UNKNOWN";
            if (evt.thread_id < 256 && registered_threads[evt.thread_id].in_use) {
                thread_name = registered_threads[evt.thread_id].name;
            }
            
            snprintf(csv_line, sizeof(csv_line), 
                     "%u,%d,%s,%s\r\n",
                     evt.timestamp_us,
                     evt.thread_id,
                     thread_name,
                     evt.event_type == GANTT_EVENT_START ? "START" : "END");
            
            send_uart_line(csv_line);
            
            atomic_inc(&gantt_ring_buffer.read_idx);
        } else {
            k_msleep(10); // Buffer vazio
        }
    }
}

/* ============================================================================
 * INICIAR THREAD DE LOGGING
 * ============================================================================
 */
void gantt_start_logger_thread(void) {
    k_thread_create(&logger_thread_data, 
                    logger_thread_stack,
                    K_THREAD_STACK_SIZEOF(logger_thread_stack),
                    logger_thread_entry,
                    NULL, NULL, NULL,
                    LOGGER_THREAD_PRIORITY, 0, K_NO_WAIT);
    
    printk("GANTT LOGGER: Background thread launched\n");
}

void gantt_set_enabled(bool enable) {
    logger_enabled = enable;
    printk("GANTT: Logging %s\n", enable ? "ENABLED" : "DISABLED");
}