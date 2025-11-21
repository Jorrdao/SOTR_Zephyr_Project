#ifndef OUTPUT_THREAD_H
#define OUTPUT_THREAD_H

// Definição da stack e prioridade da thread
// output_thread.h
#define OUTPUT_THREAD_STACK_SIZE 2048 // Pelo menos o dobro
#define OUTPUT_THREAD_PRIORITY   10 // Baixa prioridade

// Função de entrada da thread (usada em main.c)
void output_thread_entry(void *p1, void *p2, void *p3);

#endif // OUTPUT_THREAD_H