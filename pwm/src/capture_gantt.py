#!/usr/bin/env python3
"""
================================================================================
GANTT CHART GENERATOR PARA ZEPHYR RTOS
================================================================================

Este script processa os dados CSV enviados pela UART e gera um Gantt chart
mostrando a execução das threads em tempo real.

PASSOS DE UTILIZAÇÃO:
1. Conectar a board via USB
2. Executar: python3 capture_gantt.py /dev/ttyACM0 (ou COM port no Windows)
3. Deixar correr por ~10 segundos
4. Pressionar Ctrl+C
5. O script gera automaticamente gantt_chart.png

FORMATO DO CSV (recebido via UART):
timestamp_us,thread_id,thread_name,event_type
1234567,0,T_SigGen,START
1234890,0,T_SigGen,END
...

================================================================================
"""

import sys
import serial
import csv
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from datetime import datetime
import signal
import time

# ============================================================================
# CONFIGURAÇÃO
# ============================================================================

BAUD_RATE = 115200
TIMEOUT = 1.0
CSV_OUTPUT_FILE = "gantt_log.csv"
CHART_OUTPUT_FILE = "gantt_chart.png"

# Cores para cada thread (visualmente distintas)
THREAD_COLORS = {
    'T_SigGen':   '#FF6B6B',  # Vermelho (alta prioridade)
    'T_Input':    '#4ECDC4',  # Turquesa
    'T_Command':  '#45B7D1',  # Azul
    'T_Output':   '#96CEB4',  # Verde (baixa prioridade)
    'T_Logger':   '#FFEAA7',  # Amarelo (background)
    'UNKNOWN':    '#DDD'      # Cinzento
}

# ============================================================================
# CAPTURA DE DADOS VIA UART
# ============================================================================

class GanttDataCapture:
    def __init__(self, serial_port):
        self.port = serial_port
        self.serial_conn = None
        self.csv_file = None
        self.csv_writer = None
        self.running = False
        self.event_count = 0
        
    def start_capture(self):
        """Inicia a captura de dados da UART"""
        try:
            # Abrir porta série
            self.serial_conn = serial.Serial(
                self.port, 
                BAUD_RATE, 
                timeout=TIMEOUT
            )
            print(f"[OK] Conectado a {self.port} @ {BAUD_RATE} baud")
            
            # Abrir ficheiro CSV
            self.csv_file = open(CSV_OUTPUT_FILE, 'w', newline='')
            self.csv_writer = csv.writer(self.csv_file)
            
            # Escrever cabeçalho
            self.csv_writer.writerow([
                'timestamp_us', 
                'thread_id', 
                'thread_name', 
                'event_type'
            ])
            
            self.running = True
            print("[OK] A capturar dados... (Ctrl+C para parar)\n")
            
            # Loop de captura
            while self.running:
                try:
                    line = self.serial_conn.readline().decode('utf-8').strip()
                    
                    if line and not line.startswith('GANTT') and not line.startswith('timestamp'):
                        # Parsear linha CSV
                        parts = line.split(',')
                        if len(parts) == 4:
                            self.csv_writer.writerow(parts)
                            self.csv_file.flush()  # Garantir escrita imediata
                            
                            self.event_count += 1
                            if self.event_count % 100 == 0:
                                print(f"Eventos capturados: {self.event_count}")
                        else:
                            # Linha de debug/status do sistema
                            print(f"[INFO] {line}")
                
                except UnicodeDecodeError:
                    # Ignorar dados binários/corrompidos
                    pass
                except KeyboardInterrupt:
                    break
                    
        except serial.SerialException as e:
            print(f"[ERRO] Falha ao abrir porta série: {e}")
            return False
        except Exception as e:
            print(f"[ERRO] {e}")
            return False
        finally:
            self.stop_capture()
        
        return True
    
    def stop_capture(self):
        """Para a captura e fecha os recursos"""
        self.running = False
        
        if self.csv_file:
            self.csv_file.close()
            print(f"\n[OK] Dados guardados em '{CSV_OUTPUT_FILE}'")
        
        if self.serial_conn:
            self.serial_conn.close()
            print(f"[OK] Porta série fechada")
        
        print(f"[OK] Total de eventos capturados: {self.event_count}")

# ============================================================================
# GERAÇÃO DO GANTT CHART
# ============================================================================

def generate_gantt_chart(csv_file, output_file, duration_ms=10000):
    """
    Gera o Gantt chart a partir do CSV
    
    Args:
        csv_file: Ficheiro CSV com os dados
        output_file: Nome do ficheiro PNG de saída
        duration_ms: Duração a mostrar no chart (em milissegundos)
    """
    print(f"\n[PROCESSING] A gerar Gantt chart...")
    
    # Ler dados
    try:
        df = pd.read_csv(csv_file)
    except pd.errors.EmptyDataError:
        print("[ERRO] Ficheiro CSV vazio!")
        return False
    
    if df.empty:
        print("[ERRO] Nenhum dado para processar!")
        return False
    
    print(f"[OK] {len(df)} eventos carregados")
    
    # Converter timestamps para milissegundos relativos
    df['timestamp_ms'] = (df['timestamp_us'] - df['timestamp_us'].min()) / 1000.0
    
    # Filtrar pela duração desejada
    df = df[df['timestamp_ms'] <= duration_ms]
    
    # Processar eventos START/END para criar barras
    threads = df['thread_name'].unique()
    thread_executions = {thread: [] for thread in threads}
    
    # Dicionário para guardar o último START de cada thread
    last_start = {}
    
    for _, row in df.iterrows():
        thread = row['thread_name']
        
        if row['event_type'] == 'START':
            last_start[thread] = row['timestamp_ms']
        
        elif row['event_type'] == 'END' and thread in last_start:
            start_time = last_start[thread]
            end_time = row['timestamp_ms']
            duration = end_time - start_time
            
            # Adicionar execução
            thread_executions[thread].append({
                'start': start_time,
                'duration': duration
            })
            
            # Limpar
            del last_start[thread]
    
    # Criar figura
    fig, ax = plt.subplots(figsize=(14, 8))
    
    # Plotar cada thread
    thread_list = sorted(threads)
    y_positions = {thread: i for i, thread in enumerate(thread_list)}
    
    for thread in thread_list:
        y_pos = y_positions[thread]
        color = THREAD_COLORS.get(thread, THREAD_COLORS['UNKNOWN'])
        
        for execution in thread_executions[thread]:
            ax.barh(
                y_pos, 
                execution['duration'], 
                left=execution['start'], 
                height=0.8,
                color=color,
                edgecolor='black',
                linewidth=0.5
            )
    
    # Configurar eixos
    ax.set_yticks(range(len(thread_list)))
    ax.set_yticklabels(thread_list)
    ax.set_xlabel('Time (milliseconds)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Thread', fontsize=12, fontweight='bold')
    ax.set_title('Real-Time Task Execution - Gantt Chart', 
                 fontsize=14, fontweight='bold', pad=20)
    
    # Grid
    ax.grid(True, axis='x', alpha=0.3, linestyle='--')
    ax.set_xlim(0, duration_ms)
    
    # Legenda
    legend_patches = [
        mpatches.Patch(color=THREAD_COLORS.get(t, THREAD_COLORS['UNKNOWN']), 
                      label=t) 
        for t in thread_list
    ]
    ax.legend(handles=legend_patches, loc='upper right', 
             framealpha=0.9, fontsize=10)
    
    # Layout
    plt.tight_layout()
    
    # Guardar
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"[OK] Gantt chart guardado em '{output_file}'")
    
    # Mostrar estatísticas
    print("\n=== ESTATÍSTICAS DE EXECUÇÃO ===")
    for thread in thread_list:
        executions = thread_executions[thread]
        if executions:
            durations = [e['duration'] for e in executions]
            print(f"{thread}:")
            print(f"  Execuções: {len(executions)}")
            print(f"  Duração média: {sum(durations)/len(durations):.2f} ms")
            print(f"  Duração mín: {min(durations):.2f} ms")
            print(f"  Duração máx: {max(durations):.2f} ms")
    
    return True

# ============================================================================
# MAIN
# ============================================================================

def main():
    if len(sys.argv) < 2:
        print("Uso: python3 capture_gantt.py <porta_serie>")
        print("Exemplo Linux: python3 capture_gantt.py /dev/ttyACM0")
        print("Exemplo Windows: python3 capture_gantt.py COM3")
        sys.exit(1)
    
    serial_port = sys.argv[1]
    
    print("=" * 80)
    print("GANTT CHART CAPTURE - Zephyr RTOS")
    print("=" * 80)
    
    # Capturar dados
    capturer = GanttDataCapture(serial_port)
    
    # Handler para Ctrl+C
    def signal_handler(sig, frame):
        print("\n[INFO] A parar captura...")
        capturer.stop_capture()
    
    signal.signal(signal.SIGINT, signal_handler)
    
    # Iniciar captura
    if capturer.start_capture():
        # Gerar chart
        time.sleep(1)  # Dar tempo para fechar ficheiros
        generate_gantt_chart(CSV_OUTPUT_FILE, CHART_OUTPUT_FILE)
        
        print("\n" + "=" * 80)
        print("CONCLUÍDO!")
        print("=" * 80)
    else:
        print("[ERRO] Falha na captura de dados")
        sys.exit(1)

if __name__ == "__main__":
    main()
