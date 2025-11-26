#include "command_thread.h"
#include "rtdb.h"
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
    // 1. DEBUG CRÍTICO: Mostra o que chegou realmente
    // Isto vai revelar se tens espaços ou lixo antes do #
    printk("DEBUG RX: '%s'\n", cmd);

    if (strncmp(cmd, "#SF ", 4) == 0) {
        int freq = atoi(&cmd[4]);
        if (freq >= 10 && freq <= 100) {
            rtdb_set_frequency(freq);
            printk("CMD: Freq %d Hz\n", freq);
            send_uart_string("CMD OK: Freq Set\r\n");
        } else {
            send_uart_string("CMD ERR: Freq 10-100\r\n");
        }
    }
    else if (strncmp(cmd, "#SA ", 4) == 0) {
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
        snprintf(msg, sizeof(msg), "STATUS: W=%d F=%d A=%d O=%d\r\n", 
                state.wave_type, state.frequency_hz, (int)state.amplitude_v, state.output_active);
        send_uart_string(msg);
    }
    else {
        // Se falhar, dizemos o que falhou
        send_uart_string("CMD ERR: Desconhecido\r\n");
    }
}

void command_thread_entry(void *p1, void *p2, void *p3) {
    if (!device_is_ready(uart_dev)) return;

    printk("UART Ready. Waiting for $\n");

    while (1) {
        char c;
        while (uart_poll_in(uart_dev, &c) == 0) {
            // CORREÇÃO CRÍTICA: Ignorar caracteres de nova linha e retorno
            // Isto impede que o 'Enter' anterior estrague o comando seguinte
            if (c == '\n' || c == '\r') {
                continue; 
            }

            if (c == '$') {
                rx_buf[rx_pos] = '\0';
                process_command(rx_buf);
                rx_pos = 0; // Reset limpo
            } 
            else if (rx_pos < CMD_BUF_SIZE - 1) {
                rx_buf[rx_pos++] = c;
            } 
            else {
                rx_pos = 0; // Overflow reset
            }
        }
        k_msleep(20); 
    }
}