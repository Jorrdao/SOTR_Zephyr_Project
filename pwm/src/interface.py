import serial
import time
import threading
import sys

# --- CONFIGURAÇÃO ---
# Se estiveres em Linux, confirma a porta com: ls /dev/ttyACM*
SERIAL_PORT = '/dev/ttyACM0' 
# Se for Windows, muda para 'COMx'
BAUD_RATE = 115200

def calculate_checksum(cmd_content):
    """Calcula o XOR checksum igual ao firmware"""
    xor_sum = 0
    for char in cmd_content:
        xor_sum ^= ord(char)
    return xor_sum

def listen_for_acks(ser):
    """Thread que fica à escuta da placa"""
    while True:
        try:
            if ser.in_waiting:
                line = ser.readline().decode(errors='ignore').strip()
                if line:
                    # Cores para facilitar leitura
                    if "ACK" in line:
                        print(f" [BOARD] {line}")
                    elif "NACK" in line or "ERR" in line:
                        print(f" [BOARD] {line}")
                    else:
                        print(f" [BOARD] {line}")
        except OSError:
            break

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"\n--- SOTR Interface Conectada a {SERIAL_PORT} ---")
        print("Escreve o comando LIMPO (ex: SF 50).")
        print("Eu adiciono '#', '*' e Checksum automaticamente.")
        print("Escreve 'exit' para sair.\n")

        # Arranca a thread de leitura
        t = threading.Thread(target=listen_for_acks, args=(ser,), daemon=True)
        t.start()

        while True:
            # 1. Ler input do utilizador
            cmd_body = input("PC > ")
            
            if cmd_body.lower() in ['exit', 'quit']:
                break
            if not cmd_body:
                continue

            # 2. Calcular Checksum e Formatar
            cs = calculate_checksum(cmd_body)
            packet = f"#{cmd_body}*{cs:02X}$"

            # 3. Enviar byte a byte com micro-pausa (Pacing)
            # Isto evita que o buffer do Zephyr engasgue
            for byte in packet:
                ser.write(byte.encode())
                time.sleep(0.005) # 5ms entre caracteres
            
            # Pequena pausa extra depois de enviar tudo
            time.sleep(0.2)
            
            # Pequena pausa para a thread de leitura não atropelar o input visual
            time.sleep(0.2)

    except serial.SerialException:
        print(f"\n[ERRO] Não consigo abrir a porta {SERIAL_PORT}.")
        print("1. Verifica se o terminal do VS Code está fechado (ele ocupa a porta).")
        print("2. Verifica se a porta é ttyACM0 ou ttyACM1.")
    except KeyboardInterrupt:
        print("\nA sair...")

if __name__ == "__main__":
    main()