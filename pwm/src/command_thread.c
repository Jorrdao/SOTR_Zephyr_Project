#include "command_thread.h"
#include "rtdb.h"
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h> // Necessário para isprint()

#define CMD_BUF_SIZE 64
static const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
static char rx_buf[CMD_BUF_SIZE];
static int rx_pos = 0;

void send_uart_string(const char *str) {
    for (int i = 0; i < strlen(str); i++) {
        uart_poll_out(uart_dev, str[i]);
    }
}

void process_command(char *cmd) {
    printk("DEBUG RX: '%s'\n", cmd); // Mostra o que chegou limpo

    if (strncmp(cmd, "#SF ", 4) == 0) {
        int freq = atoi(&cmd[4]);
        if (freq >= 10 && freq <= 100) {
            rtdb_set_frequency(freq);
            send_uart_string("CMD OK: Freq Set\r\n");
        } else {
            send_uart_string("CMD ERR: Freq 10-100\r\n");
        }
    }
    else if (strncmp(cmd, "#SA ", 4) == 0) {
        // Tenta converter virgula em ponto se o user se enganar (hack rapido)
        char *ptr = &cmd[4];
        while(*ptr) { if(*ptr == ',') *ptr = '.'; ptr++; }

        float amp = (float)strtof(&cmd[4], NULL);
        if (amp >= 0.0f && amp <= 2.5f) {
            rtdb_set_amplitude(amp);
            send_uart_string("CMD OK: Amp Set\r\n");
        } else {
            send_uart_string("CMD ERR: Amp 0.0-2.5\r\n");
        }
    }
    else if (strncmp(cmd, "#SO ", 4) == 0) {
        if (strncmp(&cmd[4], "ON", 2) == 0) {
            rtdb_set_output_active(true);
            send_uart_string("CMD OK: Output ON\r\n");
        } else if (strncmp(&cmd[4], "OFF", 3) == 0) {
            rtdb_set_output_active(false);
            send_uart_string("CMD OK: Output OFF\r\n");
        } else {
            send_uart_string("CMD ERR: ON/OFF\r\n");
        }
    }
    else if (strncmp(cmd, "#QS", 3) == 0) {
        rtdb_state_t state = rtdb_get_state_copy();
        char msg[64];
        // Casts explícitos para evitar erros de formatação
        int amp_int = (int)state.amplitude_v; 
        int amp_dec = (int)((state.amplitude_v - amp_int) * 100);
        
        snprintf(msg, sizeof(msg), "STATUS: W=%d F=%d A=%d.%02d O=%d\r\n", 
                state.wave_type, state.frequency_hz, amp_int, amp_dec, state.output_active);
        send_uart_string(msg);
    }
    else {
        send_uart_string("CMD ERR: ???\r\n");
    }
}

void command_thread_entry(void *p1, void *p2, void *p3) {
    if (!device_is_ready(uart_dev)) {
        printk("UART Device Error\n");
        return;
    }

    printk("UART Ready. Waiting for $\n");

    while (1) {
        char c;
        while (uart_poll_in(uart_dev, &c) == 0) {
            
            // 1. Tratamento de Backspace (Apagar erros)
            if (c == '\b' || c == 0x7F) {
                if (rx_pos > 0) {
                    rx_pos--;
                    // Opcional: Enviar backspace visual para o terminal (Eco)
                    // uart_poll_out(uart_dev, '\b'); 
                    // uart_poll_out(uart_dev, ' '); 
                    // uart_poll_out(uart_dev, '\b');
                }
                continue;
            }

            // 2. Filtro de Lixo (Ignora Newlines, Aspas, etc)
            if (c == '\n' || c == '\r' || c == '\"' || c == '\'') {
                continue;
            }

            // 3. Reset Inteligente: Se recebermos um '#' e já tivermos lixo, limpamos.
            // Excepção: Se rx_pos for 0, é o inicio normal.
            if (c == '#' && rx_pos > 0) {
                printk("UART WARN: Buffer sujo limpo automaticamente.\n");
                rx_pos = 0; // Reinicia o comando
            }

            // 4. Processamento
            if (c == '$') {
                rx_buf[rx_pos] = '\0'; // Terminar string
                process_command(rx_buf);
                rx_pos = 0; // Reset buffer
            } 
            else if (rx_pos < CMD_BUF_SIZE - 1) {
                // Só aceita caracteres imprimíveis
                if (isprint((int)c)) {
                    rx_buf[rx_pos++] = c;
                }
            } 
            else {
                // Buffer cheio - limpar para evitar bloqueio
                printk("UART ERR: Buffer Full! Resetting.\n");
                rx_pos = 0;
            }
        }
        // Sleep muito curto para ser responsivo
        k_msleep(1); 
    }
}