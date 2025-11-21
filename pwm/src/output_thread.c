#include "output_thread.h"
#include "rtdb.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

// Definições de GPIO (Deve ser ajustado conforme o seu Device Tree)
// Estas definições são apenas placeholders
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)
#define LED4_NODE DT_ALIAS(led4)

static const struct gpio_dt_spec led2_spec = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec led3_spec = GPIO_DT_SPEC_GET(LED3_NODE, gpios);
static const struct gpio_dt_spec led4_spec = GPIO_DT_SPEC_GET(LED4_NODE, gpios);

void output_thread_entry(void *p1, void *p2, void *p3) {
    // 1. Inicialização dos LEDs
    if (!device_is_ready(led2_spec.port) || !device_is_ready(led3_spec.port) || !device_is_ready(led4_spec.port)) {
        printk("Erro: LED device not ready.\n");
        return;
    }
    gpio_pin_configure_dt(&led2_spec, GPIO_OUTPUT);
    gpio_pin_configure_dt(&led3_spec, GPIO_OUTPUT);
    gpio_pin_configure_dt(&led4_spec, GPIO_OUTPUT);

    while (1) {
        // 2. Obter estado (Protegido pelo Mutex dentro da função)
        rtdb_state_t state = rtdb_get_state_copy();

        // 3. Atualizar LED4 (Status ON/OFF)
        gpio_pin_set(led4_spec.port, led4_spec.pin, state.output_active);

        // 4. Atualizar LEDs de Tipo de Onda (Lembrar: LED1 sacrificado)
        gpio_pin_set(led2_spec.port, led2_spec.pin, 0); // Desliga Triang.
        gpio_pin_set(led3_spec.port, led3_spec.pin, 0); // Desliga Sinusoidal

        switch (state.wave_type) {
            case WAVE_TRIANGLE:
                gpio_pin_set(led2_spec.port, led2_spec.pin, 1);
                break;
            case WAVE_SINE:
                gpio_pin_set(led3_spec.port, led3_spec.pin, 1);
                break;
            case WAVE_SQUARE:
                // Nenhum LED especial aceso, apenas LED4 se estiver ativo
                break;
        }

        printk("Output Thread: Reading RTDB and updating LEDs.\n"); 
        
        k_msleep(500);

        // Aguardar um período (baixa frequência para feedback)
        k_msleep(500);
    }
}