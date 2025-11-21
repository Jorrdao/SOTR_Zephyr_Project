#ifndef INPUT_THREAD_H
#define INPUT_THREAD_H

#include <zephyr/kernel.h>

// Definição da stack e prioridade da thread
#define INPUT_THREAD_STACK_SIZE 1024 
#define INPUT_THREAD_PRIORITY   5 // Prioridade Média (mais alta que a Output, mais baixa que a SigGen)

// Função de entrada da thread (usada em main.c)
void input_thread_entry(void *p1, void *p2, void *p3);

#endif // INPUT_THREAD_H