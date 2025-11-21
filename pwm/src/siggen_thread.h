#ifndef SIGGEN_THREAD_H
#define SIGGEN_THREAD_H

#include <zephyr/kernel.h>

// Definição da stack e prioridade da thread
#define SIGGEN_THREAD_STACK_SIZE 1024 
#define SIGGEN_THREAD_PRIORITY   2 // Prioridade MUITO ALTA (Mais baixa é mais crítica no Zephyr)

// Função de entrada da thread (usada em main.c)
void siggen_thread_entry(void *p1, void *p2, void *p3);

#endif // SIGGEN_THREAD_H