# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pwm)

# CORREÇÃO CRÍTICA: Liste explicitamente todos os ficheiros fontes.
# Isto elimina a instabilidade causada pelo FILE(GLOB) e garante
# que o Linker encontra a 'command_thread_entry'.
target_sources(app PRIVATE
    src/main.c
    src/rtdb.c
    src/output_thread.c
    src/input_thread.c
    src/siggen_thread.c
    src/command_thread.c
)