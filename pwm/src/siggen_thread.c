// siggen_thread.c

#include "siggen_thread.h"
#include "rtdb.h"
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/printk.h>

// Definir o nó do PWM usando o alias (pwmsigout)
#define PWM_NODE DT_ALIAS(pwmsigout) 

// CORREÇÃO ESSENCIAL: Usar DEVICE_DT_GET para obter o endereço do periférico PWM diretamente,
// evitando a macro PWM_DT_SPEC_GET que causa o erro.
static const struct device *pwm_dev = DEVICE_DT_GET(PWM_NODE);

// O canal que vamos usar é o canal 0, pois definimos pinctrl-0 no DT para o canal 0.
#define PWM_CHANNEL 0

// Vamos definir uma resolução fixa alta para o duty cycle
#define PWM_MAX_DUTY_CYCLES 10000 


static void update_pwm_config(rtdb_state_t state) {
    
    // 1. Verificar se o dispositivo está pronto (deve ser verificado na inicialização, mas por segurança)
    if (!device_is_ready(pwm_dev)) {
        printk("SigGen ERR: Dispositivo PWM não está pronto!\n");
        return;
    }
    
    // 2. Desligar se a saída não estiver ativa
    if (!state.output_active) {
        // Define o duty cycle como zero, mantendo o período
        pwm_set_cycles(pwm_dev, PWM_CHANNEL, 0, 0, 0); 
        return;
    }

    // A. Lógica da Frequência (Período)
    // O período da onda quadrada é inversamente proporcional à frequência desejada (em nanosegundos - ns).
    uint32_t period_ns = 1000000000UL / state.frequency_hz; 
    
    // B. Lógica da Amplitude (Duty Cycle)
    // Para a Onda Quadrada MVP: Duty Cycle fixo a 50% do período.
    uint32_t pulse_ns = period_ns / 2; // Onda quadrada ideal 50%

    // C. Aplicação
    // O PWM só deve ser setado para ondas quadradas neste MVP, 
    // ou para ondas complexas se a lógica já estivesse implementada.
    if (state.wave_type == WAVE_SQUARE) {
        int ret = pwm_set_cycles(pwm_dev, PWM_CHANNEL, period_ns, pulse_ns, 0);

        if (ret < 0) {
            printk("SigGen ERR: Falha ao setar PWM para %d Hz\n", state.frequency_hz);
        } else {
            printk("SigGen OK: Onda Quadrada de %d Hz ativada.\n", state.frequency_hz);
        }
    }
}


void siggen_thread_entry(void *p1, void *p2, void *p3) {

    // 1. Inicialização do Periférico (Verificação única)
    if (!device_is_ready(pwm_dev)) {
        printk("SigGen ERR: Dispositivo PWM não está pronto na inicialização!\n");
        return;
    }

    printk("SigGen Thread Initialized. Priority: %d\n", k_thread_priority_get(k_current_get()));

    while (1) {
        // Para o MVP Quadrado, fazemos polling periódico, embora o ideal seja usar IPC/Semaforo.
        
        rtdb_state_t current_state = rtdb_get_state_copy();
        
        // Aplica a nova configuração PWM (somente se a frequência/status mudou ou se for necessário)
        update_pwm_config(current_state);
        
        // Polling temporário (o ideal é um semáforo ou timer de alta frequência para amostragem)
        k_msleep(100); 
    }
}