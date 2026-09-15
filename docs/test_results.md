# Resultados de Teste (Estado Atual)

## Situação

Até o momento, este repositório não contém uma tabela final consolidada de resultados da campanha M6.

## Evidências já disponíveis

- Simulador com modos de falha implementado em `scripts/elm327_simulator.py`
- Logs operacionais no monitor serial via `src/main.cpp`, `lib/obd_client`, `lib/rule_engine`
- Métricas internas no motor de regras (warnings totais e flapping)

## Pendências para fechamento de M6

- Executar campanha completa dos cenários definidos em `docs/test_plan.md`
- Coletar métricas numéricas por cenário (acurácia, FPR, latência)
- Consolidar tabela final de resultados com data, ambiente e versão de firmware

## Resultados Validados em Bancada (Hardware Real ESP32)

Ambiente de teste: ESP32 DevKit 30 pinos + OLED SSD1306 128x64 bicolor + LED onboard GPIO2, conectado via Bluetooth SPP ao simulador `scripts/elm327_simulator.py`.

| Data | Versão firmware | Cenário | Resultado esperado | Resultado observado | Status |
|---|---|---|---|---|---|
| 2026-09-10 | v0.6.0 | Sem conexão | Tela `NO_CONN`, badge `SEM CONEXAO`, ícone X, retries e LED CRITICAL | Exibição limpa sem sobreposição, tentativas incrementando e LED a 2,5 Hz | ✅ Aprovado |
| 2026-09-10 | v0.6.0 | Normal (`normal`) | Conexão SPP, tela `NORMAL` com badge `OK`, telemetria live e LED heartbeat | Conexão em ~7s, telemetria fluida, amostragem adaptativa (5, 6 e 7 PIDs), LED 1 blink a cada 5s | ✅ Aprovado |
| 2026-09-10 | v0.6.0 | Termostato (`p0128`) | Histerese 3 ciclos, tela `CRITICAL`, borda piscante 2 Hz, DTC `P0128`, LED CRITICAL | Transição OK -> CRIT confirmada, borda piscando 2 Hz, P0128 centralizado, LED pisca contínuo | ✅ Aprovado |
| 2026-09-10 | v0.6.0 | Combustão Vácuo (`p0171_vac`) | `CRITICAL`, DTC `P0171`, isolamento da causa-raiz após aceleração | `LEAN_INDET` (`Sub: 3`) em idle inicial; após aceleração (>2500 RPM) e retorno a idle, atualizou para `LEAN_VACUUM` (`Sub: 1`) | ✅ Aprovado |
| 2026-09-10 | v0.6.0 | Catalisador (`p0420`) | Transição para `WARNING`, barra de aviso com `!` piscando a 1 Hz, LED WARNING | Transição OK -> WARN confirmada, `! AVISO !` piscando a 1 Hz na barra amarela, LED com 2 piscadas + pausa de 1,5s | ✅ Aprovado |
