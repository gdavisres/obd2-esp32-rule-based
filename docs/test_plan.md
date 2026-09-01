# Plano de Testes (Estado Atual)

## Objetivo

Validar comportamento integrado do firmware (conexão OBD, leitura de PIDs, motor de regras e UI/LED) para suportar fechamento da M5 e preparação da M6.

## Escopo

- Conexão e reconexão Bluetooth SPP com ELM327
- Handshake AT (`ATZ`, `ATE0`, `ATL0`, `ATS0`, `ATH0`, `ATSP0`)
- Leitura e conversão de PIDs implementados
- Classificação de severidade por subsistema
- Renderização das telas `WAITING`, `NO_CONN`, `NORMAL`, `WARNING`, `CRITICAL`
- Padrões de LED por severidade

## Ambiente sugerido

- Firmware no ESP32 via PlatformIO
- Simulador `scripts/elm327_simulator.py`
- Monitor serial em 115200

## Casos mínimos

1. **Cenário normal**
   - Simulador em `normal`
   - Esperado: severidade `OK`, tela `NORMAL`, LED heartbeat lento
2. **Falha P0128**
   - Simulador em `p0128`
   - Esperado: transição para `CRITICAL` após histerese, DTC `P0128`
3. **Falha P0171 (vácuo)**
   - Simulador em `p0171_vac`
   - Esperado: `CRITICAL` com subtipo lean compatível com vazamento
4. **Falha P0171 (injetor)**
   - Simulador em `p0171_inj`
   - Esperado: `CRITICAL` com subtipo lean compatível com injetor
5. **Falha P0172**
   - Simulador em `p0172`
   - Esperado: `CRITICAL` com DTC `P0172`
6. **Falha P0420/P0430 (degradação catalisador)**
   - Simulador em `p0420`
   - Esperado: `WARNING`/`CRITICAL` conforme contagem de transições O₂
7. **Sem conexão**
   - Simulador desligado
   - Esperado: tela `NO_CONN`, LED ligado fixo, tentativas de reconexão no log

## Métricas para M6

- Taxa de acerto por cenário
- Falsos positivos (incluindo flapping)
- Latência média por ciclo
- Estabilidade de reconexão
