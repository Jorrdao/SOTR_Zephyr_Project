#ifndef RTDB_H
#define RTDB_H

#include <zephyr/kernel.h>
#include <stdbool.h>

// Definição dos tipos de onda
typedef enum {
    WAVE_SQUARE,
    WAVE_TRIANGLE,
    WAVE_SINE
} wave_type_t;

// Estrutura do Real-Time Database (RTDB)
typedef struct {
    wave_type_t wave_type;
    float amplitude_v;
    int frequency_hz;
    bool output_active; 
} rtdb_state_t;

// Protótipo do Mutex (para uso externo)
extern struct k_mutex rtdb_mutex;

// --- Funções de Acesso (API) ---

// Getters
rtdb_state_t rtdb_get_state_copy(void);

// Setters (Input Thread)
void rtdb_set_wave_type(wave_type_t new_type);
void rtdb_toggle_output_status(void);

// Setters (Command Thread / UART)
void rtdb_set_frequency(int freq_hz);      // <--- ADICIONADO
void rtdb_set_output_active(bool active);  // <--- ADICIONADO
void rtdb_set_amplitude(float amp_v);      // <--- Vais precisar disto para o comando #SA

#endif // RTDB_H