/**
 * @file command_thread.c
 * @brief UART Command Parser Implementation.
 * * Handles UART interrupts/polling and parses commands to control the system.
 */

#include "command_thread.h"
#include "rtdb.h"
#include "gantt_logger.h"
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

uint8_t calculate_checksum(char *cmd, int len) {
    uint8_t xor_sum = 0;
    for (int i = 0; i < len; i++) xor_sum ^= cmd[i];
    return xor_sum;
}

void process_command(char *full_cmd) {
    char *start_ptr = strchr(full_cmd, '#');
    char *star_ptr = strchr(full_cmd, '*');

    if (!start_ptr || !star_ptr || star_ptr < start_ptr) {
        send_uart_string("#ERR: Format$\r\n");
        return;
    }

    char *cmd_content = start_ptr + 1;
    int content_len = star_ptr - cmd_content;
    int received_checksum = (int)strtol(star_ptr + 1, NULL, 16);
    uint8_t calculated_checksum = calculate_checksum(cmd_content, content_len);

    if (received_checksum != calculated_checksum) {
        send_uart_string("#NACK$\r\n");
        return;
    }

    *star_ptr = '\0'; 
    bool cmd_ok = false;

    /* --- COMMAND DISPATCHING --- */

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
    /* NEW: Set Wave Type (Required by Specs) */
    else if (strncmp(cmd_content, "SW ", 3) == 0) {
        int type = atoi(&cmd_content[3]);
        if (type >= 0 && type <= 2) {
            rtdb_set_wave_type((wave_type_t)type);
            cmd_ok = true;
        } else {
            send_uart_string("#ERR: Invalid Wave$\r\n");
            return;
        }
    }
    else if (strncmp(cmd_content, "QS", 2) == 0) {
        rtdb_state_t s = rtdb_get_state_copy();
        char msg[64];
        snprintf(msg, sizeof(msg), "#STATUS: W=%d F=%d A=%d O=%d$\r\n", 
                 s.wave_type, s.frequency_hz, (int)s.amplitude_v, s.output_active);
        send_uart_string(msg);
        return; 
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
    /* System Stats  */
    else if (strncmp(cmd_content, "ST", 2) == 0) {
        sys_stats_t stats = rtdb_get_stats_copy();
        char msg[160]; 
        
        snprintf(msg, sizeof(msg), 
            "#STATS (us): Sig(C)=%u, In(R)=%u, Cmd(R)=%u, Out(R)=%u$\r\n", 
            stats.wcet_siggen_us, 
            stats.resp_input_us, 
            stats.resp_cmd_us,
            stats.resp_output_us); 
        
        send_uart_string(msg);
        return;
    }

    if (cmd_ok) send_uart_string("#ACK$\r\n");
    else send_uart_string("#ERR: Param$\r\n");
}

void command_thread_entry(void *p1, void *p2, void *p3) {
    uint8_t gantt_id = gantt_register_thread("T_Cmd");
    if (!device_is_ready(uart_dev)) return;
    
    while (1) {
        char c;
        while (uart_poll_in(uart_dev, &c) == 0) {
            if (c == '$') {
                rx_buf[rx_pos] = '\0';
                
                // MEASURE RESPONSE TIME (CMD)
                uint32_t start = k_cycle_get_32();
                
                GANTT_LOG_START(gantt_id);
                process_command(rx_buf);
                GANTT_LOG_END(gantt_id);
                
                uint32_t end = k_cycle_get_32();
                rtdb_update_metric(2, end - start); // ID 2 = Cmd Response

                rx_pos = 0; 
            } else if (rx_pos < CMD_BUF_SIZE - 1) {
                rx_buf[rx_pos++] = c;
            }
        }
        k_msleep(1); 
    }
}