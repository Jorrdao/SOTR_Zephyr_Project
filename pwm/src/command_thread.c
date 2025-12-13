#include "command_thread.h"
#include "rtdb.h"
#include "gantt_logger.h"
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h> 

#define CMD_BUF_SIZE 64
static const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
static char rx_buf[CMD_BUF_SIZE];
static int rx_pos = 0;

void send_uart_string(const char *str) {
    for (int i = 0; i < strlen(str); i++) {
        uart_poll_out(uart_dev, str[i]);
    }
}

/* --- LÓGICA DO CHECKSUM (XOR) --- */
/* Calcula o XOR de todos os caracteres da string, ignorando o '#' inicial */
uint8_t calculate_checksum(char *cmd, int len) {
    uint8_t xor_sum = 0;
    // Opcional: Se o teu buffer incluir o '#', começa no índice 1. 
    // Se passares apenas o conteúdo "SF 50", começa no 0.
    // Vamos assumir que 'cmd' aponta para "SF 50" (sem #).
    for (int i = 0; i < len; i++) {
        xor_sum ^= cmd[i];
    }
    return xor_sum;
}

void process_command(char *full_cmd) {
    // full_cmd vem como: "#SF 50*30" (o $ já foi removido)
    
    // 1. Validar estrutura básica (tem de ter # e *)
    char *start_ptr = strchr(full_cmd, '#');
    char *star_ptr = strchr(full_cmd, '*');

    if (!start_ptr || !star_ptr || star_ptr < start_ptr) {
        // Formato inválido nem merece NACK, é lixo puro.
        send_uart_string("#ERR: Format$\r\n");
        return;
    }

    // 2. Isolar o conteúdo do comando ("SF 50")
    // O conteúdo começa logo a seguir ao '#' e acaba antes do '*'
    char *cmd_content = start_ptr + 1;
    int content_len = star_ptr - cmd_content;

    // 3. Extrair o Checksum Recebido (que está depois do *)
    int received_checksum = (int)strtol(star_ptr + 1, NULL, 16);

    // 4. Calcular o Checksum Real (Localmente)
    uint8_t calculated_checksum = calculate_checksum(cmd_content, content_len);

    // 5. O VEREDICTO (ACK vs NACK)
    if (received_checksum != calculated_checksum) {
        printk("UART: Checksum Fail! Rec:%02X Calc:%02X\n", received_checksum, calculated_checksum);
        // Responde NACK para o PC reenviar
        send_uart_string("#NACK$\r\n");
        return; // Pára tudo, não executa comando corrompido!
    }

    // Se chegou aqui, é seguro executar.
    // Colocamos um \0 no lugar do '*' para que as funções de string 
    // vejam apenas "SF 50" e ignorem o resto.
    *star_ptr = '\0'; 

    // --- EXECUÇÃO DO COMANDO ---
    bool cmd_ok = false;

    // Nota: Agora comparamos 'cmd_content' ("SF 50") e não 'full_cmd' ("#SF 50...")
    if (strncmp(cmd_content, "SF ", 3) == 0) {
        int freq = atoi(&cmd_content[3]);
        if (freq >= 10 && freq <= 100) {
            rtdb_set_frequency(freq);
            cmd_ok = true;
        }
    }
    else if (strncmp(cmd_content, "SA ", 3) == 0) {
        float amp = (float)strtof(&cmd_content[3], NULL);
        if (amp >= 0.0f && amp <= 2.5f) {
            rtdb_set_amplitude(amp);
            cmd_ok = true;
        }
    }
    else if (strncmp(cmd_content, "SO ", 3) == 0) {
        if (strncmp(&cmd_content[3], "ON", 2) == 0) {
            rtdb_set_output_active(true);
            cmd_ok = true;
        } else if (strncmp(&cmd_content[3], "OFF", 3) == 0) {
            rtdb_set_output_active(false);
            cmd_ok = true;
        }
    }
    else if (strncmp(cmd_content, "QS", 2) == 0) {
        // Query State é especial, responde com dados, não com ACK simples
        rtdb_state_t s = rtdb_get_state_copy();
        char msg[64];
        snprintf(msg, sizeof(msg), "#STATUS: W=%d F=%d A=%d O=%d$\r\n", 
                 s.wave_type, s.frequency_hz, (int)s.amplitude_v, s.output_active);
        send_uart_string(msg);
        return; // Já respondemos
    }
    else if (strncmp(cmd_content, "GL ", 3) == 0) {
        if (strncmp(&cmd_content[3], "ON", 2) == 0) {
            gantt_set_enabled(true);
            cmd_ok = true;
        } else if (strncmp(&cmd_content[3], "OFF", 3) == 0) {
            gantt_set_enabled(false);
            cmd_ok = true;
        }
    }

    // Resposta Final
    if (cmd_ok) {
        send_uart_string("#ACK$\r\n");
    } else {
        // Checksum estava bom, mas valores inválidos (ex: Freq 900)
        send_uart_string("#ERR: Invalid Param$\r\n");
    }
}

/* A função command_thread_entry mantém-se igual à anterior */
void command_thread_entry(void *p1, void *p2, void *p3) {

    uint8_t gantt_id = gantt_register_thread("T_Command");

    if (!device_is_ready(uart_dev)) return;
    
    while (1) {
        char c;
        while (uart_poll_in(uart_dev, &c) == 0) {

            GANTT_LOG_START(gantt_id);

            // ... (mesma lógica de buffer, backspace e deteção do $) ...
            if (c == '$') {
                rx_buf[rx_pos] = '\0';
                process_command(rx_buf);
                rx_pos = 0; 
            } else if (rx_pos < CMD_BUF_SIZE - 1) {
                rx_buf[rx_pos++] = c;
            }
            GANTT_LOG_END(gantt_id);
        }

        
        k_msleep(1); 
    }
}