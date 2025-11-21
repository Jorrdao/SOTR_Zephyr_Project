// command_thread.h

#ifndef COMMAND_THREAD_H
#define COMMAND_THREAD_H

#include <zephyr/kernel.h>

// Thread de prioridade MÉDIA/BAIXA
#define COMMAND_THREAD_STACK_SIZE 1024 
#define COMMAND_THREAD_PRIORITY   7 

// Função de entrada da thread
void command_thread_entry(void *p1, void *p2, void *p3);

#endif // COMMAND_THREAD_H