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

import time
import random
import sys
import os
import argparse
import select
import threading
import socket

# Tenta importar serial para quando usado em modo Serial
try:
    import serial
except ImportError:
    serial = None

class EngineState:
    def __init__(self, fault_mode='normal'):
        self.fault_mode = fault_mode
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
        if self.fault_mode == 'p0128':
            self.ect = 60 + random.uniform(-2, 2)  # nunca atinge 71
            self.iat = 28
        elif self.fault_mode == 'overheat':
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
        if self.fault_mode == 'p0171_vac':
            # Vazamento de vácuo: STFT alto só em idle
            if self.rpm < 1000:
                self.stft = 18 + random.randint(-3, 3)
                self.ltft = 5
            else:
                self.stft = 2 + random.randint(-2, 2)
                self.ltft = 5
        elif self.fault_mode == 'p0171_inj':
            # Injetor falhando: STFT alto em toda faixa
            self.stft = 15 + random.randint(-2, 2)
            self.ltft = 8
        elif self.fault_mode == 'p0172':
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
        if self.fault_mode == 'p0420':
            # Catalisador degradado: oscila entre 0.1 e 0.9
            self.o2_voltage = random.choice([0.1, 0.2, 0.8, 0.9])
        else:
            # Saudável: estável em 0.55
            self.o2_voltage = 0.55 + random.uniform(-0.05, 0.05)


def to_hex(value, num_bytes):
    if num_bytes == 1:
        return f"{value & 0xFF:02X}"
    elif num_bytes == 2:
        return f"{value & 0xFFFF:04X}"
    return ""


def process_command(cmd: str, state: EngineState) -> str:
    cmd = cmd.strip().upper()
    if not cmd:
        return ""
    state.update()

    resp = "NO DATA\r\r>"
    if cmd == 'ATZ':
        time.sleep(0.3)
        resp = 'ELM327 v2.1\r\r>'
    elif cmd in ('ATE0', 'ATL0', 'ATS0', 'ATH0', 'ATSP0'):
        time.sleep(0.05)
        resp = 'OK\r\r>'
    elif cmd == '0100':
        time.sleep(0.08)
        resp = '4100BE3FA813\r\r>'
    elif cmd == '0105':
        time.sleep(0.05)
        resp = f'4105{to_hex(int(state.ect + 40), 1)}\r\r>'
    elif cmd == '0106':
        time.sleep(0.05)
        resp = f'4106{to_hex(int(state.stft * 128 / 100 + 128), 1)}\r\r>'
    elif cmd == '0107':
        time.sleep(0.05)
        resp = f'4107{to_hex(int(state.ltft * 128 / 100 + 128), 1)}\r\r>'
    elif cmd == '010C':
        time.sleep(0.06)
        resp = f'410C{to_hex(int(state.rpm * 4), 2)}\r\r>'
    elif cmd == '010D':
        time.sleep(0.05)
        resp = f'410D{to_hex(int(state.speed), 1)}\r\r>'
    elif cmd == '010F':
        time.sleep(0.05)
        resp = f'410F{to_hex(int(state.iat + 40), 1)}\r\r>'
    elif cmd == '0111':
        time.sleep(0.05)
        resp = f'4111{to_hex(int(state.throttle * 255 / 100), 1)}\r\r>'
    elif cmd == '0113':
        time.sleep(0.05)
        resp = '411303\r\r>'
    elif cmd == '0115':
        time.sleep(0.06)
        o2_v_byte = int(state.o2_voltage / 0.005) & 0xFF
        o2_trim_byte = int(state.o2_trim * 128 / 100 + 128) & 0xFF
        resp = f'4115{to_hex(o2_v_byte, 1)}{to_hex(o2_trim_byte, 1)}\r\r>'

    print(f"[SIM] RX: {cmd:<6} -> TX: {resp.replace(chr(13), ' ').replace('>', '').strip()}")
    return resp


def stdin_reader(state: EngineState):
    """Lê throttle do stdin (0-100) sem bloquear o loop principal."""
    while True:
        try:
            if select.select([sys.stdin], [], [], 0)[0]:
                line = sys.stdin.readline().strip()
                if line:
                    try:
                        t = int(line)
                        state.user_throttle = max(0, min(100, t))
                        print(f"[SIM] >>> Throttle ajustado para {state.user_throttle}% (RPM alvo: ~{800 + state.user_throttle*50}) <<<")
                    except ValueError:
                        pass
        except Exception:
            pass
        time.sleep(0.1)


def handle_client_socket(sock, state: EngineState):
    buffer = ""
    sock.settimeout(0.5)
    while True:
        try:
            data = sock.recv(1024)
            if not data:
                print("[SIM] ESP32 desconectou.")
                break
            buffer += data.decode('ascii', errors='ignore')
            while '\r' in buffer:
                cmd, buffer = buffer.split('\r', 1)
                if cmd.strip():
                    resp = process_command(cmd, state)
                    if resp:
                        sock.sendall(resp.encode('ascii'))
        except (socket.timeout, TimeoutError):
            continue
        except (ConnectionResetError, BrokenPipeError, OSError):
            print("[SIM] Conexão com ESP32 encerrada.")
            break
    try:
        sock.close()
    except Exception:
        pass
    print("[SIM] Aguardando nova conexão do ESP32...")


def run_bluetooth_server(state: EngineState, channel: int = 1):
    # Tenta usar DBus para registrar o perfil SPP (UUID 0x1101) no BlueZ
    try:
        import dbus
        import dbus.service
        import dbus.mainloop.glib
        from gi.repository import GLib

        dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
        bus = dbus.SystemBus()
        manager = dbus.Interface(bus.get_object('org.bluez', '/org/bluez'), 'org.bluez.ProfileManager1')

        class SPPProfile(dbus.service.Object):
            def __init__(self, bus, path):
                super().__init__(bus, path)

            @dbus.service.method("org.bluez.Profile1", in_signature="", out_signature="")
            def Release(self):
                print("[SIM] BlueZ Profile liberado.")

            @dbus.service.method("org.bluez.Profile1", in_signature="oha{sv}", out_signature="")
            def NewConnection(self, path, fd, properties):
                sock_fd = fd.take()
                sock = socket.fromfd(sock_fd, socket.AF_BLUETOOTH, socket.SOCK_STREAM)
                print(f"\n[SIM] >>> ESP32 CONECTADO com sucesso via Bluetooth SPP ({path}) <<<")
                client_thread = threading.Thread(target=handle_client_socket, args=(sock, state), daemon=True)
                client_thread.start()

            @dbus.service.method("org.bluez.Profile1", in_signature="o", out_signature="")
            def RequestDisconnection(self, path):
                print(f"[SIM] Desconexão requisitada para {path}")

        profile_path = '/obd2/elm327_spp'
        profile = SPPProfile(bus, profile_path)
        opts = {
            'Channel': dbus.UInt16(channel),
            'AutoConnect': True,
            'Name': 'SerialPort',
            'Role': 'server',
            'RequireAuthentication': False,
            'RequireAuthorization': False,
        }
        manager.RegisterProfile(profile_path, '00001101-0000-1000-8000-00805f9b34fb', opts)
        print(f"[SIM] Perfil Bluetooth SPP (UUID 0x1101) registrado com sucesso no BlueZ!")
        print(f"[SIM] Servidor ativo aguardando o ESP32 (Modo: {state.fault_mode})...")
        print("[SIM] Digite um valor 0-100 no stdin para controlar o throttle")

        loop = GLib.MainLoop()
        loop.run()
    except Exception as e:
        print(f"[SIM] DBus ProfileManager indisponível ({e}). Tentando socket RFCOMM direto...")
        server_sock = socket.socket(socket.AF_BLUETOOTH, socket.SOCK_STREAM, socket.BTPROTO_RFCOMM)
        server_sock.bind(("00:00:00:00:00:00", channel))
        server_sock.listen(1)
        print(f"[SIM] Socket RFCOMM escutando no canal {channel}...")
        while True:
            client_sock, client_info = server_sock.accept()
            print(f"\n[SIM] >>> ESP32 conectado: {client_info} <<<")
            handle_client_socket(client_sock, state)


def run_serial_server(state: EngineState, port: str, baud: int):
    if serial is None:
        print("[SIM] ERRO: Módulo pyserial não instalado. Execute: pip install pyserial")
        sys.exit(1)

    try:
        ser = serial.Serial(port, baud, timeout=1)
        print(f"[SIM] ELM327 V2.1 em {port} @ {baud} baud (Modo: {state.fault_mode})")
        print("[SIM] Digite um valor 0-100 no stdin para controlar o throttle")

        buffer = ''
        while True:
            if ser.in_waiting > 0:
                ch = ser.read(ser.in_waiting).decode('ascii', errors='ignore')
                buffer += ch
                while '\r' in buffer:
                    cmd, buffer = buffer.split('\r', 1)
                    if cmd.strip():
                        response = process_command(cmd, state)
                        if response:
                            ser.write(response.encode('ascii'))
            time.sleep(0.01)
    except serial.SerialException as e:
        print(f"[SIM] Erro na porta serial {port}: {e}")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Simulador ELM327 V2.1 com injeção de falhas OBD-II")
    parser.add_argument('-b', '--bluetooth', action='store_true',
                        help='Executa como servidor Bluetooth RFCOMM (padrão se nenhum --port for passado)')
    parser.add_argument('-p', '--port', type=str, default=os.environ.get('PORT', None),
                        help='Porta serial física/virtual (ex: /dev/ttyUSB0)')
    parser.add_argument('--baud', type=int, default=int(os.environ.get('BAUD', '38400')),
                        help='Baud rate para modo serial (padrão: 38400)')
    parser.add_argument('-f', '--fault', type=str, default=os.environ.get('FAULT_MODE', 'normal'),
                        choices=['normal', 'p0128', 'p0171_vac', 'p0171_inj', 'p0172', 'p0420', 'overheat'],
                        help='Modo de injeção de falha')

    args = parser.parse_args()

    # Seleciona modo: se explicitamente passado --port ou variável PORT, usa Serial; senão Bluetooth
    use_bluetooth = args.bluetooth or (args.port is None and os.environ.get('MODE', '').lower() in ('bt', 'bluetooth', ''))

    state = EngineState(fault_mode=args.fault)
    print(f"[SIM] ========================================")
    print(f"[SIM] Simulador ELM327 v2.1 — UNIFEI / PET-TEC")
    print(f"[SIM] Modo de falha: {args.fault}")
    print(f"[SIM] ========================================")

    threading.Thread(target=stdin_reader, args=(state,), daemon=True).start()

    try:
        if use_bluetooth:
            run_bluetooth_server(state, channel=1)
        else:
            port = args.port if args.port else '/dev/ttyUSB0'
            run_serial_server(state, port, args.baud)
    except KeyboardInterrupt:
        print("\n[SIM] Encerrando simulador...")


if __name__ == '__main__':
    main()