#include "output_thread.h"
#include "rtdb.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>


#define LED_SQUARE_NODE   DT_ALIAS(ledsquare)   // Era o led0/led1
#define LED_TRIANGLE_NODE DT_ALIAS(ledtriangle) // Era o led1/led2
#define LED_SINE_NODE     DT_ALIAS(ledsine)     // Era o led2/led3
#define LED_ACTIVE_NODE   DT_ALIAS(ledactive)   // Era o led3/led4

static const struct gpio_dt_spec led_sq_spec = GPIO_DT_SPEC_GET(LED_SQUARE_NODE, gpios);
static const struct gpio_dt_spec led_tri_spec = GPIO_DT_SPEC_GET(LED_TRIANGLE_NODE, gpios);
static const struct gpio_dt_spec led_sin_spec = GPIO_DT_SPEC_GET(LED_SINE_NODE, gpios);
static const struct gpio_dt_spec led_act_spec = GPIO_DT_SPEC_GET(LED_ACTIVE_NODE, gpios);

void output_thread_entry(void *p1, void *p2, void *p3) {
    // Validação de segurança
    if (!device_is_ready(led_sq_spec.port) || !device_is_ready(led_tri_spec.port) ||
        !device_is_ready(led_sin_spec.port) || !device_is_ready(led_act_spec.port)) {
        printk("Erro: Um ou mais LEDs não estão prontos.\n");
        return;
    }

    // Configuração
    gpio_pin_configure_dt(&led_sq_spec, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_tri_spec, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_sin_spec, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_act_spec, GPIO_OUTPUT_INACTIVE);

    while (1) {
        // Obter cópia segura do estado
        rtdb_state_t state = rtdb_get_state_copy();

        // 1. Atualizar LED de Status Geral (ON/OFF)
        gpio_pin_set_dt(&led_act_spec, state.output_active);

        // 2. Resetar todos os LEDs de tipo de onda
        gpio_pin_set_dt(&led_sq_spec, 0);
        gpio_pin_set_dt(&led_tri_spec, 0);
        gpio_pin_set_dt(&led_sin_spec, 0);

        // 3. Ligar APENAS o LED correspondente
        // Nota: Só mostramos o tipo se o output estiver ativo? O PDF não especifica,
        // mas geralmente mostra-se a configuração mesmo com output OFF.
        switch (state.wave_type) {
            case WAVE_SQUARE:
                gpio_pin_set_dt(&led_sq_spec, 1); // AGORA SIM, CUMPRIU A SPEC
                break;
            case WAVE_TRIANGLE:
                gpio_pin_set_dt(&led_tri_spec, 1);
                break;
            case WAVE_SINE:
                gpio_pin_set_dt(&led_sin_spec, 1);
                break;
        }

        // Debugging Output
        //printk("[%u] OUTPUT (Prio 10): A atualizar LEDs...\n", k_uptime_get_32());
        k_msleep(500);

        // Frequência de atualização de 10Hz é suficiente para UI.
        // 1000ms (1s) é demasiado lento, o utilizador sente lag ao carregar no botão.
        k_msleep(100); 
    }
}