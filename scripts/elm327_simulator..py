#!/usr/bin/env python3
"""
Simulador ELM327 V2.1 para desenvolvimento do firmware M2-M6.

Emula o comportamento de um ELM327 via Serial-USB. Responde a todos os
PIDs do projeto com valores plausíveis e variáveis.

PIDs suportados (respondem com dados):
    0x00 (bitmask), 0x05 (ECT), 0x06 (STFT), 0x07 (LTFT),
    0x0C (RPM), 0x0D (Speed), 0x0F (IAT), 0x11 (TP),
    0x13 (O2 sensors present), 0x15 (O2 sensor 2)

Resposta à bitmask 0x00: BE 3F A8 13
    - 0x05, 0x06, 0x07 suportados (byte 3F)
    - 0x0C, 0x0D, 0x0F suportados (byte 3F)
    - 0x11, 0x13 suportados (byte A8 = bit 4 + bit 6... verificar)
    - 0x15 NÃO suportado na bitmask 01-20 (precisa pedir 0x20)
    Para simplificar, simulamos todos como suportados.
"""

import serial
import time
import random
import sys

PORT = '/dev/ttyUSB0'
BAUD = 38400

# Estado do simulador (persiste entre comandos)
class EngineState:
    def __init__(self):
        self.rpm = 800       # idle
        self.speed = 0
        self.ect = 30        # frio
        self.iat = 25
        self.throttle = 5    # idle
        self.stft = 0
        self.ltft = 0
        self.o2_voltage = 0.5
        self.o2_trim = 0
        self.tick = 0

    def update(self):
        """Atualiza valores a cada tick para simular motor funcionando."""
        self.tick += 1

        # Aquece o motor gradualmente até 90°C
        if self.ect < 90:
            self.ect += 0.5
        if self.iat < 35:
            self.iat += 0.1

        # Variações suaves
        self.rpm += random.randint(-30, 30)
        self.rpm = max(700, min(1500, self.rpm))

        self.throttle += random.randint(-2, 2)
        self.throttle = max(3, min(15, self.throttle))

        # Fuel trims oscilam
        self.stft += random.randint(-3, 3)
        self.stft = max(-10, min(10, self.stft))

        # O2 sensor oscila entre lean/rich (0.1-0.9 V)
        self.o2_voltage = random.uniform(0.1, 0.9)


state = EngineState()


def to_hex(value, num_bytes):
    """Converte valor para string hex com número fixo de bytes."""
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

    # Modo 01 - PID 0x00 (bitmask de PIDs suportados 01-20)
    if cmd == '0100':
        time.sleep(0.1)
        # Bitmask: BE 3F A8 13 (PIDs 05, 06, 07, 0C, 0D, 0F, 11, 13 suportados)
        return '4100BE3FA813\r\r>'

    # PID 0x05 - ECT (A - 40 = °C)
    if cmd == '0105':
        time.sleep(0.1)
        ect_byte = int(state.ect + 40) & 0xFF
        return f'4105{to_hex(ect_byte, 1)}\r\r>'

    # PID 0x06 - STFT ((A - 128) * 100/128 = %)
    if cmd == '0106':
        time.sleep(0.1)
        stft_byte = int(state.stft * 128 / 100 + 128) & 0xFF
        return f'4106{to_hex(stft_byte, 1)}\r\r>'

    # PID 0x07 - LTFT (mesma fórmula)
    if cmd == '0107':
        time.sleep(0.1)
        ltft_byte = int(state.ltft * 128 / 100 + 128) & 0xFF
        return f'4107{to_hex(ltft_byte, 1)}\r\r>'

    # PID 0x0C - RPM (((A*256)+B)/4)
    if cmd == '010C':
        time.sleep(0.12)
        rpm_raw = int(state.rpm * 4)
        return f'410C{to_hex(rpm_raw, 2)}\r\r>'

    # PID 0x0D - Speed (A km/h)
    if cmd == '010D':
        time.sleep(0.08)
        return f'410D{to_hex(state.speed, 1)}\r\r>'

    # PID 0x0F - IAT (A - 40)
    if cmd == '010F':
        time.sleep(0.1)
        iat_byte = int(state.iat + 40) & 0xFF
        return f'410F{to_hex(iat_byte, 1)}\r\r>'

    # PID 0x11 - Throttle Position (A * 100/255)
    if cmd == '0111':
        time.sleep(0.1)
        tp_byte = int(state.throttle * 255 / 100) & 0xFF
        return f'4111{to_hex(tp_byte, 1)}\r\r>'

    # PID 0x13 - O2 sensors present (bitmask, 1 byte)
    if cmd == '0113':
        time.sleep(0.1)
        # Bit 0 = sensor 1 presente, bit 1 = sensor 2 presente
        return '411303\r\r>'

    # PID 0x15 - O2 sensor 2 (A=V*200, B=trim)
    if cmd == '0115':
        time.sleep(0.12)
        o2_v_byte = int(state.o2_voltage / 0.005) & 0xFF
        o2_trim_byte = int(state.o2_trim * 128 / 100 + 128) & 0xFF
        return f'4115{to_hex(o2_v_byte, 1)}{to_hex(o2_trim_byte, 1)}\r\r>'

    # PID não suportado
    return 'NO DATA\r\r>'


def main():
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1)
        print(f"[SIM] Simulador ELM327 V2.1 ativo em {PORT} @ {BAUD} baud")
        print("[SIM] PIDs suportados: 00, 05, 06, 07, 0C, 0D, 0F, 11, 13, 15")
        print("[SIM] Aguardando comandos...")

        buffer = ''
        while True:
            if ser.in_waiting > 0:
                ch = ser.read(ser.in_waiting).decode('ascii', errors='ignore')
                buffer += ch

                while '\r' in buffer:
                    cmd, buffer = buffer.split('\r', 1)
                    if cmd:
                        print(f"[SIM] >>> {cmd}")
                        response = process_command(cmd)
                        print(f"[SIM] <<< {response!r}")
                        ser.write(response.encode('ascii'))

    except serial.SerialException as e:
        print(f"[SIM] Erro serial: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n[SIM] Encerrando...")
        ser.close()


if __name__ == '__main__':
    main()