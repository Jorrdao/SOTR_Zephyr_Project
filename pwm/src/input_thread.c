#include "input_thread.h"
#include "rtdb.h"
#include "gantt_logger.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

/* --- DEFINIÇÕES DOS BOTÕES (Aliases do DeviceTree) --- */
static const struct gpio_dt_spec but1 = GPIO_DT_SPEC_GET(DT_ALIAS(but1), gpios);
static const struct gpio_dt_spec but2 = GPIO_DT_SPEC_GET(DT_ALIAS(but2), gpios);
static const struct gpio_dt_spec but3 = GPIO_DT_SPEC_GET(DT_ALIAS(but3), gpios);
static const struct gpio_dt_spec but4 = GPIO_DT_SPEC_GET(DT_ALIAS(but4), gpios);

/* --- ESTRUTURA DE CALLBACK --- */
static struct gpio_callback button_cb_data;

/* --- SEMÁFORO E FLAG DE EVENTOS --- */
K_SEM_DEFINE(input_sem, 0, 1);
static volatile uint32_t button_event_mask = 0;

/* --- HANDLER DE INTERRUPÇÃO (ISR) --- */
void button_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    // Identificar qual botão foi pressionado e guardar na máscara
    if (pins & BIT(but1.pin)) { button_event_mask |= BIT(0); }
    if (pins & BIT(but2.pin)) { button_event_mask |= BIT(1); }
    if (pins & BIT(but3.pin)) { button_event_mask |= BIT(2); }
    if (pins & BIT(but4.pin)) { button_event_mask |= BIT(3); }

    // Acordar a thread
    k_sem_give(&input_sem);
}

/* --- LÓGICA DOS BOTÕES --- */

// BUT1: Mudar Tipo de Onda (Square -> Triangle -> Sine)
void handle_but1(void) {
    rtdb_state_t s = rtdb_get_state_copy();
    wave_type_t next = WAVE_SQUARE;
    
    if (s.wave_type == WAVE_SQUARE) next = WAVE_TRIANGLE;
    else if (s.wave_type == WAVE_TRIANGLE) next = WAVE_SINE;
    else next = WAVE_SQUARE;
    
    rtdb_set_wave_type(next);
    printk("[INPUT] Wave Type changed to %d\n", next);
}

// BUT2: Mudar Frequência (+10Hz, wrap 90->10)
void handle_but2(void) {
    rtdb_state_t s = rtdb_get_state_copy();
    int next_freq = s.frequency_hz + 10;
    if (next_freq > 90) next_freq = 10; // Volta a 10 se passar de 90
    
    rtdb_set_frequency(next_freq);
    printk("[INPUT] Frequency changed to %d Hz\n", next_freq);
}

// BUT3: Toggle Output (ON/OFF)
void handle_but3(void) {
    rtdb_state_t s = rtdb_get_state_copy();
    bool next_state = !s.output_active;
    
    rtdb_set_output_active(next_state);
    printk("[INPUT] Output %s\n", next_state ? "ON" : "OFF");
}

// BUT4: Mudar Amplitude (+0.5V, wrap 2.5 -> 0)
void handle_but4(void) {
    rtdb_state_t s = rtdb_get_state_copy();
    float next_amp = s.amplitude_v + 0.5f;
    if (next_amp > 2.5f) next_amp = 0.0f; // Volta a 0 se passar de 2.5
    
    rtdb_set_amplitude(next_amp);
    // Print float com casting para int para evitar erros se printf float não estiver ativo
    printk("[INPUT] Amplitude changed to %d.%d V\n", (int)next_amp, (int)((next_amp - (int)next_amp)*10));
}

/* --- FUNÇÃO AUXILIAR DE CONFIGURAÇÃO --- */
int configure_button(const struct gpio_dt_spec *but) {
    if (!device_is_ready(but->port)) return -1;
    int ret = gpio_pin_configure_dt(but, GPIO_INPUT | GPIO_PULL_UP);
    if (ret < 0) return ret;
    ret = gpio_pin_interrupt_configure_dt(but, GPIO_INT_EDGE_TO_ACTIVE);
    return ret;
}

/* --- THREAD PRINCIPAL --- */
void input_thread_entry(void *p1, void *p2, void *p3) {
    uint8_t gantt_id = gantt_register_thread("T_Input");

    // 1. Configurar Hardware
    if (configure_button(&but1) < 0 || configure_button(&but2) < 0 ||
        configure_button(&but3) < 0 || configure_button(&but4) < 0) {
        printk("Error configuring buttons!\n");
        return;
    }

    // 2. Registar Callbacks (Uma função para todos)
    gpio_init_callback(&button_cb_data, button_handler, 
                       BIT(but1.pin) | BIT(but2.pin) | BIT(but3.pin) | BIT(but4.pin));
    
    gpio_add_callback(but1.port, &button_cb_data);
    // Assumindo que estão na mesma porta (Port 0). Se estiverem em portas diferentes,
    // precisarias de callbacks separados, mas na nRF52-DK estão todos na Port 0.

    printk("Input Thread ready. Buttons 1-4 active.\n");

    while (1) {
        // Espera por interrupção
        k_sem_take(&input_sem, K_FOREVER);

        // Debounce simples (espera 50ms para o sinal estabilizar)
        k_msleep(50);

        GANTT_LOG_START(gantt_id);

        // Verificar flags atómicas e limpar
        uint32_t events = button_event_mask;
        button_event_mask = 0; // Reset flags

        if (events & BIT(0)) handle_but1();
        if (events & BIT(1)) handle_but2();
        if (events & BIT(2)) handle_but3();
        if (events & BIT(3)) handle_but4();

        GANTT_LOG_END(gantt_id);
    }
}