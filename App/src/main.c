#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/devicetree.h>
#include <stdbool.h>
#include <stdio.h>

// --- I. DEFINIÇÕES RTOS E IPC ---

// Tipos de Onda
enum wave_type { WAVE_SQUARE, WAVE_TRIANGLE, WAVE_SINE };

// Estrutura do Estado (RTDB)
struct system_state_t {
    float amplitude;
    int frequency;
    enum wave_type type;
    bool output_on;
};

// Instância Global do RTDB e Mutex (Para Resource Protection)
struct system_state_t rt_db = {
    .amplitude = 0.0f,
    .frequency = 10,
    .type = WAVE_SQUARE,
    .output_on = false
};
K_MUTEX_DEFINE(rt_db_mutex);

// Definição do Semáforo (Para notificar o SigGen - Concurrency Management)
K_SEM_DEFINE(siggen_update_sem, 0, 1);

// --- II. DEFINIÇÕES DE HARDWARE (SEM MATRIZES GLOBAIS) ---

// Node IDs (Usando os aliases corrigidos do overlay: hífens)
#define PWM_NODE                     DT_ALIAS(siggenpwm)
#define LED_SQUARE_NODE              DT_ALIAS(ledsquare)
#define LED_TRIANGLE_NODE            DT_ALIAS(ledtriangle)
#define LED_SINE_NODE                DT_ALIAS(ledsine)
#define LED_STATUS_NODE              DT_ALIAS(ledstatus)
#define BTN_WAVE_TYPE_NODE           DT_ALIAS(btnwavetype)
#define BTN_FREQUENCY_NODE           DT_ALIAS(btnfrequency)
#define BTN_OUTPUT_TOGGLE_NODE       DT_ALIAS(btnoutputtoggle)
#define BTN_AMPLITUDE_NODE           DT_ALIAS(btnamplitude)

// Obter as estruturas Device Tree estáticas (Global)
static const struct pwm_dt_spec pwm_dev_spec = PWM_DT_SPEC_GET(PWM_NODE);

// LEDs (Obter a especificação do dispositivo diretamente)
static const struct gpio_dt_spec led_square = GPIO_DT_SPEC_GET(LED_SQUARE_NODE, gpios);
static const struct gpio_dt_spec led_triangle = GPIO_DT_SPEC_GET(LED_TRIANGLE_NODE, gpios);
static const struct gpio_dt_spec led_sine = GPIO_DT_SPEC_GET(LED_SINE_NODE, gpios);
static const struct gpio_dt_spec led_status = GPIO_DT_SPEC_GET(LED_STATUS_NODE, gpios);

// Botões
static const struct gpio_dt_spec btn_wave_type = GPIO_DT_SPEC_GET(BTN_WAVE_TYPE_NODE, gpios);
static const struct gpio_dt_spec btn_frequency = GPIO_DT_SPEC_GET(BTN_FREQUENCY_NODE, gpios);
static const struct gpio_dt_spec btn_output_toggle = GPIO_DT_SPEC_GET(BTN_OUTPUT_TOGGLE_NODE, gpios);
static const struct gpio_dt_spec btn_amplitude = GPIO_DT_SPEC_GET(BTN_AMPLITUDE_NODE, gpios);


// --- III. THREADS (DEFINIÇÃO E LÓGICA DE STUB) ---

#define STACK_SIZE 1024
#define SIGGEN_PRIORITY -1      // Mais Alta/Time-Critical
#define INPUT_PRIORITY 1        
#define COMMAND_PRIORITY 10     
#define OUTPUT_PRIORITY 15      

// Lógica básica da Thread SigGen
void siggen_thread_entry(void *p1, void *p2, void *p3) {
    printk("SigGen Thread (Prio: %d) iniciada.\n", SIGGEN_PRIORITY);
    while (1) {
        k_sem_take(&siggen_update_sem, K_FOREVER);
        
        // Secção Crítica: Leitura Rápida do RTDB
        if (k_mutex_lock(&rt_db_mutex, K_FOREVER) == 0) {
            struct system_state_t state_copy = rt_db;
            k_mutex_unlock(&rt_db_mutex);

            // TODO: Adicionar lógica de cálculo PWM/DAC aqui
        }
    }
}

// Stubs para as outras threads
void input_update_thread_entry(void *p1, void *p2, void *p3) { 
    printk("Input Update Thread (Prio: %d) iniciada.\n", INPUT_PRIORITY);
    while (1) { k_sleep(K_MSEC(100)); } 
}

void command_processor_thread_entry(void *p1, void *p2, void *p3) {
    printk("Command Processor Thread (Prio: %d) iniciada.\n", COMMAND_PRIORITY);
    while (1) { k_sleep(K_SECONDS(1)); }
}

void output_update_thread_entry(void *p1, void *p2, void *p3) {
    printk("Output Update Thread (Prio: %d) iniciada.\n", OUTPUT_PRIORITY);
    while (1) { k_sleep(K_MSEC(50)); }
}

// Definição das Threads (Inicialização Automática)
K_THREAD_DEFINE(siggen_tid, STACK_SIZE, siggen_thread_entry, NULL, NULL, NULL, SIGGEN_PRIORITY, 0, 0);
K_THREAD_DEFINE(input_tid, STACK_SIZE, input_update_thread_entry, NULL, NULL, NULL, INPUT_PRIORITY, 0, 0);
K_THREAD_DEFINE(command_tid, STACK_SIZE, command_processor_thread_entry, NULL, NULL, NULL, COMMAND_PRIORITY, 0, 0);
K_THREAD_DEFINE(output_tid, STACK_SIZE, output_update_thread_entry, NULL, NULL, NULL, OUTPUT_PRIORITY, 0, 0);


// --- IV. FUNÇÃO PRINCIPAL (INICIALIZAÇÃO SEQUENCIAL) ---

void main(void)
{
    int ret;
    
    printk("--- SOTR Instrumentação: Inicialização do Sistema ---\n");
    
    // --- 1. Verificação e Configuração de Saídas (LEDs) ---
    // Verificamos a porta apenas uma vez e configuramos cada LED sequencialmente.
    if (!device_is_ready(led_square.port)) {
        printk("FATAL: Driver GPIO (Porta de LEDs) não está pronto!\n");
        return;
    }

    ret = gpio_pin_configure_dt(&led_square, GPIO_OUTPUT_ACTIVE);
    ret += gpio_pin_configure_dt(&led_triangle, GPIO_OUTPUT_ACTIVE);
    ret += gpio_pin_configure_dt(&led_sine, GPIO_OUTPUT_ACTIVE);
    ret += gpio_pin_configure_dt(&led_status, GPIO_OUTPUT_ACTIVE);

    if (ret < 0) {
        printk("FATAL: Erro ao configurar LEDs!\n");
        return;
    }
    
    // Desligar todos os LEDs
    gpio_pin_set_dt(&led_square, 0);
    gpio_pin_set_dt(&led_triangle, 0);
    gpio_pin_set_dt(&led_sine, 0);
    gpio_pin_set_dt(&led_status, 0);


    // --- 2. Configuração de Entradas (Botões) ---
    ret = 0;
    ret += gpio_pin_configure_dt(&btn_wave_type, GPIO_INPUT | GPIO_PULL_UP);
    ret += gpio_pin_configure_dt(&btn_frequency, GPIO_INPUT | GPIO_PULL_UP);
    ret += gpio_pin_configure_dt(&btn_output_toggle, GPIO_INPUT | GPIO_PULL_UP);
    ret += gpio_pin_configure_dt(&btn_amplitude, GPIO_INPUT | GPIO_PULL_UP);

    if (ret < 0) {
        printk("FATAL: Erro ao configurar Botões!\n");
        return;
    }
    
    // --- 3. Inicialização do PWM ---
    if (!device_is_ready(pwm_dev_spec.dev)) {
        printk("FATAL: Driver PWM não está pronto!\n");
        return;
    }
    
    printk("INICIALIZAÇÃO BEM SUCEDIDA. 4 Threads RTOS ativadas.\n");
    // O controlo retorna, e o kernel escala as threads.
}