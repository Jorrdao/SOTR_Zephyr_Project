#include "siggen_thread.h"
#include "rtdb.h"
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/printk.h>

/* Configurações do PWM Carrier */
#define PWM_NODE DT_ALIAS(signal_pwm) // Usa o ALIAS que criaste no overlay!
static const struct pwm_dt_spec pwm_dev = PWM_DT_SPEC_GET(PWM_NODE);

/* Frequência base do PWM (Carrier) - deve ser > 10x a frequência máxima de amostragem */
#define PWM_CARRIER_NS  20000 // 50kHz (20µs period)

/* Variáveis de Controlo Globais (volatile porque são usadas no timer) */
static volatile int sample_index = 0;
static volatile wave_type_t current_wave_type = WAVE_SQUARE;
static volatile uint32_t current_amp_scale = 100; // 0 a 100%

/* Tabela LUT (coloca a tabela completa aqui) */
static const float sine_lut[100] = { /* ... valores acima ... */ };

/* Timer do Zephyr para amostragem */
struct k_timer sample_timer;

/* --- Callback do Timer (AQUI ACONTECE A MAGIA) --- */
void sample_timer_handler(struct k_timer *dummy) {
    uint32_t pulse_ns = 0;

    switch (current_wave_type) {
        case WAVE_SINE:
            // Ler da tabela e escalar pelo periodo do PWM
            pulse_ns = (uint32_t)(sine_lut[sample_index] * PWM_CARRIER_NS);
            break;

        case WAVE_TRIANGLE:
            // Podes criar uma LUT para triângulo ou calcular matematicamente (subir/descer)
            // Exemplo simplificado (usar LUT seria melhor para performance):
            if (sample_index < 50) 
                pulse_ns = (sample_index * 2 * PWM_CARRIER_NS) / 100;
            else 
                pulse_ns = ((100 - sample_index) * 2 * PWM_CARRIER_NS) / 100;
            break;

        case WAVE_SQUARE:
        default:
            // Quadrada fixa: 50% metade do tempo, 0% na outra (ou fixo se quiseres DC puro)
            if (sample_index < 50) pulse_ns = PWM_CARRIER_NS; // High
            else pulse_ns = 0; // Low
            break;
    }

    // Aplicar Amplitude (Escala simples)
    // pulse_ns = (pulse_ns * current_amp_scale) / 100; // Se quiseres implementar controlo de volume

    // Atualizar Hardware
    pwm_set_dt(&pwm_dev, PWM_CARRIER_NS, pulse_ns);

    // Avançar índice (Circular 0-99)
    sample_index = (sample_index + 1) % 100;
}

/* --- Thread Principal de Controlo --- */
void siggen_thread_entry(void *p1, void *p2, void *p3) {
    if (!pwm_is_ready_dt(&pwm_dev)) {
        printk("Erro: PWM device not ready.\n");
        return;
    }

    // Inicializar Timer
    k_timer_init(&sample_timer, sample_timer_handler, NULL);

    int64_t last_print_time = 0;

    while (1) {
        // 1. Ler RTDB (Baixa frequência, só para ver se o utilizador mudou algo)
        rtdb_state_t state = rtdb_get_state_copy();

        // 2. Atualizar Variáveis Globais para o Timer usar
        current_wave_type = state.wave_type;

        if (state.output_active) {
            // Calcular velocidade do timer baseado na frequência desejada
            // Periodo do Timer = 1 / (Freq_Sinal * Num_Amostras)
            // Ex: 50Hz * 100 amostras = 5000Hz -> 200us
            
            // Proteção contra divisão por zero
            if (state.frequency_hz < 1) state.frequency_hz = 1;

            uint32_t sampling_period_us = 1000000 / (state.frequency_hz * 100);
            
            // Ajustar o timer (start se estiver parado ou ajustar periodo)
            k_timer_start(&sample_timer, K_USEC(sampling_period_us), K_USEC(sampling_period_us));
        } else {
            k_timer_stop(&sample_timer);
            pwm_set_dt(&pwm_dev, PWM_CARRIER_NS, 0); // Forçar 0V
        }

        //Debugging Output
        int64_t now = k_uptime_get(); // Usa k_uptime_get() que retorna int64_t para evitar overflow rápido
        if (now - last_print_time >= 5000) { 
            //printk("[%lld] SIGGEN (Prio 2): Ainda estou vivo! (Freq: %d Hz)\n", now, state.frequency_hz);
            last_print_time = now;
        }


        k_msleep(100); // Verificar mudanças no RTDB a cada 100ms
    }
}