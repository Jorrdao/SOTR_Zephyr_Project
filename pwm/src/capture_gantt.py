import sys
import serial
import csv
import pandas as pd
import matplotlib.pyplot as plt
import time
import threading

# --- CONFIGURAÇÃO ---
CSV_OUTPUT_FILE = "gantt_log.csv"
CHART_OUTPUT_FILE = "gantt_chart.png"
BAUD_RATE = 115200

# --- HELPER: Envio Lento (A CORREÇÃO CRÍTICA) ---
def send_packet_slow(ser, cmd):
    """Calcula checksum e envia byte-a-byte com pausa para não bloquear a UART"""
    xor = 0
    for c in cmd: xor ^= ord(c)
    packet = f"#{cmd}*{xor:02X}$"
    
    print(f"[TX] >> {packet}")
    for byte in packet:
        ser.write(byte.encode())
        time.sleep(0.005) # <--- O SEGREDO: 5ms de pausa evita Buffer Overflow no Zephyr

# --- HELPER: Tarefa de Injeção de Comandos ---
def injector_task(ser):
    # Espera inicial para garantir que o logging arrancou
    time.sleep(1.5) 
    
    # 1. Mudar para 80Hz (Para veres barras muito juntas no gráfico)
    print("\n[INJECTOR] A mudar para 80Hz...")
    send_packet_slow(ser, "SF 80")
    
    time.sleep(2)
    # 2. Mudar Amplitude (Só para validar comando)
    print("[INJECTOR] A mudar Amplitude...")
    send_packet_slow(ser, "SA 1.0")
    
    time.sleep(2)
    # 3. Mudar para 20Hz (Para veres barras afastadas no gráfico)
    print("[INJECTOR] A mudar para 20Hz...")
    send_packet_slow(ser, "SF 20")
    
    time.sleep(2)
    print("[INJECTOR] Fim do teste. A desligar logs...")
    send_packet_slow(ser, "GL OFF")

# --- HELPER: Gerar Gráfico ---
def generate_gantt_chart(csv_file, output_file):
    try:
        df = pd.read_csv(csv_file)
        if df.empty:
            print("[GRAPH] AVISO: O CSV está vazio. O comando #GL ON falhou?")
            return
    except:
        print("[GRAPH] Erro ao ler CSV.")
        return

    # Limpeza de dados
    df = df[pd.to_numeric(df['timestamp_us'], errors='coerce').notnull()]
    df['timestamp_us'] = df['timestamp_us'].astype(float)
    if df.empty: return

    # Normalizar tempo para começar em 0 segundos
    start_time = df['timestamp_us'].min()
    df['rel_time_s'] = (df['timestamp_us'] - start_time) / 1e6

    fig, ax = plt.subplots(figsize=(14, 6))
    
    # Cores para cada thread
    color_map = {
        'T_SigGen': 'tab:red',
        'T_Input': 'tab:green',
        'T_Command': 'tab:blue',   # Ajusta o nome conforme o teu registo no C
        'T_Cmd': 'tab:blue',       # Caso tenhas mudado o nome
        'T_Output': 'tab:orange'
    }

    # Desenhar barras
    # width=0.005 (5ms) garante que vês traços finos e não blocos sólidos
    for i, row in df.iterrows():
        name = row['thread_name']
        if row['event_type'] == 'START':
            c = color_map.get(name, 'gray')
            ax.barh(name, 0.005, left=row['rel_time_s'], height=0.6, color=c)

    ax.set_xlabel('Tempo (segundos)')
    ax.set_title('Diagrama de Gantt - Execução em Tempo Real')
    ax.grid(True, axis='x', linestyle='--', alpha=0.5)
    plt.tight_layout()
    plt.savefig(output_file, dpi=150)
    print(f"[GRAPH] Gráfico guardado em: {output_file}")

# --- MAIN ---
def main():
    if len(sys.argv) < 2:
        print("Uso: python3 capture_gantt.py /dev/ttyACM0")
        return

    port = sys.argv[1]
    
    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=1)
        print(f"[INIT] Conectado a {port}")
        
        # 1. ATIVAR LOGGING (Usando envio lento para garantir que a placa aceita!)
        print("[INIT] A ativar Gantt Logger...")
        send_packet_slow(ser, "GL ON")
        
        # 2. Iniciar Injetor em paralelo
        t = threading.Thread(target=injector_task, args=(ser,), daemon=True)
        t.start()
        
        # 3. Gravar Dados
        with open(CSV_OUTPUT_FILE, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['timestamp_us', 'thread_id', 'thread_name', 'event_type'])
            
            print("[REC] A capturar dados... Aguarda ~8 segundos.")
            start_time = time.time()
            last_pkt_time = time.time()
            
            while True:
                line = ser.readline().decode(errors='ignore').strip()
                if line:
                    last_pkt_time = time.time()
                    parts = line.split(',')
                    # Só guarda se for CSV válido (ignora ACKs e NACKs)
                    if len(parts) == 4 and parts[0].isdigit():
                        writer.writerow(parts)
                
                # Para se houver silêncio (GL OFF aceite) ou timeout
                if (time.time() - last_pkt_time > 2.0 and time.time() - start_time > 3):
                    print("[FIM] Captura terminada.")
                    break
                    
    except Exception as e:
        print(f"Erro: {e}")
        return

    generate_gantt_chart(CSV_OUTPUT_FILE, CHART_OUTPUT_FILE)

if __name__ == "__main__":
    main()