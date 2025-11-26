#include "rtdb.h"

// Definição global do Mutex (visível externamente através do rtdb.h)
K_MUTEX_DEFINE(rtdb_mutex);

// Instância única do RTDB
static rtdb_state_t rtdb_current_state = {
    .wave_type = WAVE_SQUARE,
    .amplitude_v = 0.0f,
    .frequency_hz = 10,
    .output_active = false
};

void rtdb_set_amplitude(float amp_v) {
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    // Validação: 0V a 2.5V (PDF pag 2)
    if (amp_v < 0.0f) amp_v = 0.0f;
    if (amp_v > 2.5f) amp_v = 2.5f;
    
    rtdb_current_state.amplitude_v = amp_v;
    k_mutex_unlock(&rtdb_mutex);
}

void rtdb_set_frequency(int freq_hz) {
    k_mutex_lock(&rtdb_mutex, K_FOREVER);

    // Validação de segurança (Opcional mas recomendada em RTOS)
    // O PDF diz 10Hz a 100Hz. Vamos garantir que não entramos em zonas instáveis.
    if (freq_hz < 1) freq_hz = 1; 
    if (freq_hz > 100) freq_hz = 100; // Limite do PDF

    rtdb_current_state.frequency_hz = freq_hz;
    
    // Debug (opcional, cuidado com o excesso de logs em produção)
    // printk("RTDB: Frequency updated to %d Hz\n", freq_hz);

    k_mutex_unlock(&rtdb_mutex);
}

void rtdb_set_output_active(bool active) {
    k_mutex_lock(&rtdb_mutex, K_FOREVER);

    rtdb_current_state.output_active = active;

    k_mutex_unlock(&rtdb_mutex);
}
// --- Funções de SET (Escrita) ---

void rtdb_set_wave_type(wave_type_t new_type) {
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    
    // Secção Crítica
    rtdb_current_state.wave_type = new_type;
    
    k_mutex_unlock(&rtdb_mutex);
    // TODO: Adicionar aqui lógica de notificação (Semaforo) para T_SigGen e T_OutputUpdate
}

void rtdb_toggle_output_status() {
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    
    // Secção Crítica
    rtdb_current_state.output_active = !rtdb_current_state.output_active;
    
    k_mutex_unlock(&rtdb_mutex);
}

// --- Função de GET (Leitura) ---

// Retorna uma CÓPIA do estado atual, garantindo atomicidade na leitura
rtdb_state_t rtdb_get_state_copy(void) {
    rtdb_state_t copy;
    
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    
    // Secção Crítica: Cópia do estado
    copy = rtdb_current_state;
    
    k_mutex_unlock(&rtdb_mutex);
    
    return copy;
}