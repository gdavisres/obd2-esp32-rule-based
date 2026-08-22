#!/usr/bin/env python3
"""
Simulador ELM327 V2.1 com modos de falha para teste do motor de regras (M4-M6).

Modos de operação (alterar via variável FAULT_MODE):
    'normal'        — motor quente, sem falhas
    'p0128'         — termostato aberto (ECT nunca passa de 65°C)
    'p0171_vac'     — vazamento de vácuo (STFT alto só em idle)
    'p0171_inj'     — injetor falhando (STFT alto em toda faixa)
    'p0172'         — rich (STFT muito negativo)
    'p0420'         — catalisador degradado (O2 downstream oscila)
    'overheat'      — superaquecimento (ECT > 105°C)
"""

import serial
import time
import random
import sys
import os

PORT = '/dev/ttyUSB0'
BAUD = 38400

# Modo de falha (sobrescrever via variável de ambiente FAULT_MODE)
FAULT_MODE = os.environ.get('FAULT_MODE', 'normal')

class EngineState:
    def __init__(self):
        self.rpm = 800
        self.speed = 0
        self.ect = 30
        self.iat = 25
        self.throttle = 5
        self.stft = 0
        self.ltft = 0
        self.o2_voltage = 0.55
        self.o2_trim = 0
        self.tick = 0
        self.user_throttle = 0  # 0-100%, controlado via stdin

    def update(self):
        self.tick += 1

        # Aquece gradualmente (exceto em modo P0128)
        if FAULT_MODE == 'p0128':
            self.ect = 60 + random.uniform(-2, 2)  # nunca atinge 71
            self.iat = 28
        elif FAULT_MODE == 'overheat':
            self.ect = 108 + random.uniform(-1, 1)
            self.iat = 40
        else:
            if self.ect < 92:
                self.ect += 0.5
            if self.iat < 35:
                self.iat += 0.1

        # RPM baseado em throttle do usuário (simula acelerador)
        target_rpm = 800 + (self.user_throttle * 50)
        self.rpm += (target_rpm - self.rpm) * 0.1
        self.rpm += random.randint(-20, 20)
        self.rpm = max(700, min(6000, self.rpm))

        self.throttle = max(3, self.user_throttle + random.randint(-1, 1))

        # Combustão conforme modo de falha
        if FAULT_MODE == 'p0171_vac':
            # Vazamento de vácuo: STFT alto só em idle
            if self.rpm < 1000:
                self.stft = 18 + random.randint(-3, 3)
                self.ltft = 5
            else:
                self.stft = 2 + random.randint(-2, 2)
                self.ltft = 5
        elif FAULT_MODE == 'p0171_inj':
            # Injetor falhando: STFT alto em toda faixa
            self.stft = 15 + random.randint(-2, 2)
            self.ltft = 8
        elif FAULT_MODE == 'p0172':
            # Rich
            self.stft = -18 + random.randint(-3, 3)
            self.ltft = -10
        else:
            # Normal
            self.stft += random.randint(-3, 3)
            self.stft = max(-8, min(8, self.stft))
            self.ltft += random.randint(-1, 1)
            self.ltft = max(-3, min(3, self.ltft))

        # O2 downstream
        if FAULT_MODE == 'p0420':
            # Catalisador degradado: oscila entre 0.1 e 0.9
            self.o2_voltage = random.choice([0.1, 0.2, 0.8, 0.9])
        else:
            # Saudável: estável em 0.55
            self.o2_voltage = 0.55 + random.uniform(-0.05, 0.05)


state = EngineState()
print(f"[SIM] Modo de falha: {FAULT_MODE}")


def to_hex(value, num_bytes):
    if num_bytes == 1:
        return f"{value & 0xFF:02X}"
    elif num_bytes == 2:
        return f"{value & 0xFFFF:04X}"
    return ""


def process_command(cmd: str) -> str:
    cmd = cmd.strip().upper()
    state.update()

    if cmd == 'ATZ':
        time.sleep(0.5)
        return 'ELM327 v2.1\r\r>'
    if cmd in ('ATE0', 'ATL0', 'ATS0', 'ATH0', 'ATSP0'):
        time.sleep(0.05)
        return 'OK\r\r>'

    if cmd == '0100':
        time.sleep(0.1)
        return '4100BE3FA813\r\r>'
    if cmd == '0105':
        time.sleep(0.1)
        return f'4105{to_hex(int(state.ect + 40), 1)}\r\r>'
    if cmd == '0106':
        time.sleep(0.1)
        return f'4106{to_hex(int(state.stft * 128 / 100 + 128), 1)}\r\r>'
    if cmd == '0107':
        time.sleep(0.1)
        return f'4107{to_hex(int(state.ltft * 128 / 100 + 128), 1)}\r\r>'
    if cmd == '010C':
        time.sleep(0.12)
        return f'410C{to_hex(int(state.rpm * 4), 2)}\r\r>'
    if cmd == '010D':
        time.sleep(0.08)
        return f'410D{to_hex(state.speed, 1)}\r\r>'
    if cmd == '010F':
        time.sleep(0.1)
        return f'410F{to_hex(int(state.iat + 40), 1)}\r\r>'
    if cmd == '0111':
        time.sleep(0.1)
        return f'4111{to_hex(int(state.throttle * 255 / 100), 1)}\r\r>'
    if cmd == '0113':
        time.sleep(0.1)
        return '411303\r\r>'
    if cmd == '0115':
        time.sleep(0.12)
        o2_v_byte = int(state.o2_voltage / 0.005) & 0xFF
        o2_trim_byte = int(state.o2_trim * 128 / 100 + 128) & 0xFF
        return f'4115{to_hex(o2_v_byte, 1)}{to_hex(o2_trim_byte, 1)}\r\r>'

    return 'NO DATA\r\r>'


def stdin_reader():
    """Lê throttle do stdin (0-100) sem bloquear o loop principal."""
    import select
    while True:
        if select.select([sys.stdin], [], [], 0)[0]:
            line = sys.stdin.readline().strip()
            try:
                t = int(line)
                state.user_throttle = max(0, min(100, t))
                print(f"[SIM] Throttle set to {state.user_throttle}%")
            except ValueError:
                pass
        time.sleep(0.1)


def main():
    import threading
    threading.Thread(target=stdin_reader, daemon=True).start()

    try:
        ser = serial.Serial(PORT, BAUD, timeout=1)
        print(f"[SIM] ELM327 V2.1 em {PORT} @ {BAUD} baud (modo: {FAULT_MODE})")
        print("[SIM] Digite um valor 0-100 no stdin para controlar o throttle")

        buffer = ''
        while True:
            if ser.in_waiting > 0:
                ch = ser.read(ser.in_waiting).decode('ascii', errors='ignore')
                buffer += ch
                while '\r' in buffer:
                    cmd, buffer = buffer.split('\r', 1)
                    if cmd:
                        response = process_command(cmd)
                        ser.write(response.encode('ascii'))
    except serial.SerialException as e:
        print(f"[SIM] Erro: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n[SIM] Encerrando...")


if __name__ == '__main__':
    main()