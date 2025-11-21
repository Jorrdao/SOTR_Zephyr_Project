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

// Protótipo do Mutex (para uso externo - não é comum, mas útil aqui)
extern struct k_mutex rtdb_mutex;

// Funções protegidas de Acesso ao RTDB (Protótipos)
// A thread de Input usará estas funções para alterar o estado
void rtdb_set_wave_type(wave_type_t new_type);
void rtdb_toggle_output_status();

// A thread de Output usará esta função para ler o estado
rtdb_state_t rtdb_get_state_copy(void);