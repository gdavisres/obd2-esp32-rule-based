# Arquitetura Atual do Sistema

## Visão geral

O firmware está organizado em três camadas principais:

1. **Aquisição (OBD-II)**
   - Biblioteca: `lib/obd_client`
   - Responsabilidades: conexão Bluetooth SPP com ELM327, inicialização AT, handshake de PIDs suportados, leitura e conversão de PIDs SAE J1979.
2. **Diagnóstico (motor de regras)**
   - Biblioteca: `lib/rule_engine`
   - Responsabilidades: avaliar snapshot OBD, aplicar FSM por subsistema e histerese, produzir severidade e DTC.
3. **Apresentação (UI e alerta local)**
   - Biblioteca: `lib/display` + lógica em `src/main.cpp`
   - Responsabilidades: renderizar telas OLED por estado, sinalizar severidade via LED.

## Fluxo de dados

1. `setup()` inicializa display, cliente OBD e motor de regras.
2. `loop()` executa `obd_client_loop()` para manter FSM de conexão.
3. Quando conectado, `obd_client_read_all_supported_pids()` gera `ObdSnapshot`.
4. `rule_engine_evaluate()` transforma snapshot em `RuleEngineSnapshot`.
5. `main.cpp` escolhe tela (`NORMAL`, `WARNING`, `CRITICAL`, `WAITING`, `NO_CONN`) e padrão de LED conforme severidade/estado.

## Componentes principais

- `include/obd_pids.h`: enum de PIDs, estrutura `ObdSnapshot`, contratos de conversão.
- `include/obd_config.h`: parâmetros de timeout/reconexão do cliente OBD.
- `include/thresholds.h`: thresholds de diagnóstico e parâmetros de histerese.
- `lib/obd_client/obd_client.cpp`: FSM de conexão + parser/conversão + política de sampling.
- `lib/rule_engine/rule_engine.cpp`: lógica das 3 FSMs (termostato, combustão, catalisador).
- `lib/display/display.cpp`: renderização SSD1306 thread-safe com mutex FreeRTOS.
- `scripts/elm327_simulator.py`: simulador para cenários normal e falhas (P0128/P0171/P0172/P0420).

## Estado de maturidade

- Base funcional de M0 a M4 concluída.
- Integração operacional da UI já existente e em ajuste fino (M5).
- Validação sistemática com métricas finais permanece para M6.
