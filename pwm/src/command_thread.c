// command_thread.c

#include "command_thread.h"
#include <zephyr/sys/printk.h>

void command_thread_entry(void *p1, void *p2, void *p3) {
    printk("T_CommandParser: Thread UART iniciada. Aguardando comandos...\n");
    
    // TODO: Implementar a lógica de receção UART e parsing de comandos aqui.
    
   while (1) {
        // CORREÇÃO: Usar k_sleep() que aceita k_timeout_t (K_FOREVER).
        k_sleep(K_FOREVER); 
    }
}