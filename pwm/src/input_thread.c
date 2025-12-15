/**
 * @file input_thread.c
 * @brief Input Thread Implementation.
 * * Handles 4 buttons via interrupts and updates RTDB.
 * * Includes visual feedback via printk (UART) for user interaction.
 */

#include "input_thread.h"
#include "rtdb.h"
#include "gantt_logger.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

/* --- DEVICE TREE ALIASES --- */
static const struct gpio_dt_spec but1 = GPIO_DT_SPEC_GET(DT_ALIAS(but1), gpios);
static const struct gpio_dt_spec but2 = GPIO_DT_SPEC_GET(DT_ALIAS(but2), gpios);
static const struct gpio_dt_spec but3 = GPIO_DT_SPEC_GET(DT_ALIAS(but3), gpios);
static const struct gpio_dt_spec but4 = GPIO_DT_SPEC_GET(DT_ALIAS(but4), gpios);

static struct gpio_callback button_cb_data;
K_SEM_DEFINE(input_sem, 0, 1);
static volatile uint32_t button_event_mask = 0;

/* --- INTERRUPT HANDLER --- */
void button_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    if (pins & BIT(but1.pin)) { button_event_mask |= BIT(0); }
    if (pins & BIT(but2.pin)) { button_event_mask |= BIT(1); }
    if (pins & BIT(but3.pin)) { button_event_mask |= BIT(2); }
    if (pins & BIT(but4.pin)) { button_event_mask |= BIT(3); }
    k_sem_give(&input_sem);
}

/* --- BUTTON LOGIC WITH FEEDBACK --- */

// SW1: Wave Type
void handle_but1(void) { 
    wave_type_t current = rtdb_get_state_copy().wave_type;
    wave_type_t next = (current == WAVE_SQUARE) ? WAVE_TRIANGLE : (current == WAVE_TRIANGLE ? WAVE_SINE : WAVE_SQUARE);
    rtdb_set_wave_type(next);
    
    // Feedback Strings
    const char *names[] = {"SQUARE", "TRIANGLE", "SINE"};
    printk("[INPUT] Wave: %s\n", names[next]);
}

// SW2: Frequency
void handle_but2(void) { 
    int current = rtdb_get_state_copy().frequency_hz;
    int next = current + 10;
    if (next > 90) next = 10;
    
    rtdb_set_frequency(next);
    printk("[INPUT] Freq: %d Hz\n", next);
}

// SW3: Output Toggle
void handle_but3(void) { 
    bool next = !rtdb_get_state_copy().output_active;
    rtdb_set_output_active(next); 
    printk("[INPUT] Output: %s\n", next ? "ON" : "OFF");
}

// SW4: Amplitude
void handle_but4(void) { 
    float current = rtdb_get_state_copy().amplitude_v;
    float next = current + 0.5f;
    if (next > 2.5f) next = 0.0f;
    
    rtdb_set_amplitude(next);
    
    // Print hack para evitar erros de float no printf se não estiver configurado
    printk("[INPUT] Amp: %d.%d V\n", (int)next, (int)((next - (int)next) * 10)); 
}

/* --- CONFIGURATION --- */
int configure_button(const struct gpio_dt_spec *but) {
    if (!device_is_ready(but->port)) return -1;
    int ret = gpio_pin_configure_dt(but, GPIO_INPUT | GPIO_PULL_UP);
    if (ret < 0) return ret;
    return gpio_pin_interrupt_configure_dt(but, GPIO_INT_EDGE_TO_ACTIVE);
}

/* --- THREAD ENTRY POINT --- */
void input_thread_entry(void *p1, void *p2, void *p3) {
    uint8_t gantt_id = gantt_register_thread("T_Input");

    if (configure_button(&but1) < 0 || configure_button(&but2) < 0 ||
        configure_button(&but3) < 0 || configure_button(&but4) < 0) {
        printk("INPUT FATAL: Failed to config buttons\n");
        return;
    }

    gpio_init_callback(&button_cb_data, button_handler, 
                       BIT(but1.pin) | BIT(but2.pin) | BIT(but3.pin) | BIT(but4.pin));
    gpio_add_callback(but1.port, &button_cb_data);

    printk("INPUT: Buttons active.\n");

    while (1) {
        k_sem_take(&input_sem, K_FOREVER);
        
        // START PROFILING
        uint32_t start = k_cycle_get_32();

        k_msleep(50); // Debounce
        
        GANTT_LOG_START(gantt_id);
        uint32_t events = button_event_mask;
        button_event_mask = 0; 

        if (events & BIT(0)) handle_but1();
        if (events & BIT(1)) handle_but2();
        if (events & BIT(2)) handle_but3();
        if (events & BIT(3)) handle_but4();
        
        GANTT_LOG_END(gantt_id);

        // END PROFILING
        uint32_t end = k_cycle_get_32();
        rtdb_update_metric(1, end - start); // ID 1 = Input Response Time
    }
}