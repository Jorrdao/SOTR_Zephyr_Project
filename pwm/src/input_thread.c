// input_thread.c

#include "input_thread.h"
#include "rtdb.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include "gantt_logger.h" 
// Definição do nó do BUT1
#define BUT1_NODE DT_ALIAS(but1) 
static const struct gpio_dt_spec but1_spec = GPIO_DT_SPEC_GET(BUT1_NODE, gpios);

// Semáforo para notificar a thread quando o BUT1 é pressionado
K_SEM_DEFINE(but1_sem, 0, 1); 

// Estrutura de callback deve ser declarada estaticamente
static struct gpio_callback but1_cb_data;

// Callback de Interrupção para o BUT1
// CORRETO:
void but1_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    k_sem_give(&but1_sem);
}

// Lógica de alternar o tipo de onda
static void handle_wave_type_toggle(void)
{
    rtdb_state_t current_state = rtdb_get_state_copy();
    wave_type_t new_type;
    
    // Lógica de alternância: Square -> Triangle -> Sine -> Square
    switch (current_state.wave_type) {
        case WAVE_SQUARE:
            new_type = WAVE_TRIANGLE;
            break;
        case WAVE_TRIANGLE:
            new_type = WAVE_SINE;
            break;
        case WAVE_SINE:
            new_type = WAVE_SQUARE;
            break;
        default:
            new_type = WAVE_SQUARE; 
    }
    
    // Atualiza o estado no RTDB, protegido pelo Mutex.
    rtdb_set_wave_type(new_type);
    printk("Input Thread: Wave type toggled to %d\n", new_type);
}


void input_thread_entry(void *p1, void *p2, void *p3) {
    uint8_t gantt_id = gantt_register_thread("T_Input");
    // 1. Configuração do Pino (Verificação e Modo)
    if (!device_is_ready(but1_spec.port)) {
        printk("Erro: BUT1 device not ready.\n");
        return;
    }
    
    // 1.1 Configurar o pino como entrada
    // O GPIO_PULL_UP é fundamental para botões ligados a GND.
    int ret = gpio_pin_configure_dt(&but1_spec, GPIO_INPUT | GPIO_PULL_UP);
    if (ret < 0) {
        printk("Erro: Falha na configuracao do BUT1 (%d)\n", ret);
        return;
    }

    // 1.2 Configurar o pino para gerar interrupção (borda ativa é a descida, pois PULL_UP está ON)
    ret = gpio_pin_interrupt_configure_dt(&but1_spec, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        printk("Erro: Falha na configuracao da interrupcao (%d)\n", ret);
        return;
    }

    // 1.3 Inicializar a estrutura de callback
    // Usa but1_cb_data, a estrutura estática que declaramos.
    gpio_init_callback(&but1_cb_data, 
                       but1_handler, 
                       BIT(but1_spec.pin));

    // 1.4 Adicionar o handler à interrupção
    gpio_add_callback(but1_spec.port, &but1_cb_data);


    printk("Input Thread Initialized. Waiting for BUT1 press...\n");

    while (1) {
        // Espera de forma síncrona pelo sinal do semáforo, liberando a CPU para outras threads.
        k_sem_take(&but1_sem, K_FOREVER);
        GANTT_LOG_START(gantt_id);
        // Debugging Output
        //printk("[%u] INPUT  (Prio 5): Botão detetado. A processar...\n", k_uptime_get_32());
        
        
        // Processar o evento fora do contexto de interrupção (ISR)
        handle_wave_type_toggle();
        GANTT_LOG_START(gantt_id);
        // TODO: Adicionar lógica de anti-debounce (k_msleep curto ou timer) se necessário.
    }
}