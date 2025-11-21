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