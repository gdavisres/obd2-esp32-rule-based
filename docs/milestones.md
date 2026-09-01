# Milestones do Projeto

Estado consolidado do firmware em 2026-09.

| Milestone | Descrição | Status atual | Evidência principal |
| --- | --- | --- | --- |
| Etapa 0 | Setup PlatformIO + VS Code | ✅ concluído | `platformio.ini` |
| Etapa 1 | Estrutura Git/GitHub | ✅ concluído | repositório versionado |
| M0 | Boot inicial + LED/OLED smoke test | ✅ concluído | `src/main.cpp` (boot e logs) |
| M1 | Driver SSD1306 + telas base | ✅ concluído | `lib/display/*` |
| M2 | Bluetooth SPP + init ELM327 | ✅ concluído | `lib/obd_client/*` (FSM conexão + sequência AT) |
| M3 | Camada OBD-II + parser + snapshot | ✅ concluído | `include/obd_pids.h`, `lib/obd_client/obd_client.cpp` |
| M4 | Motor de regras (3 subsistemas) + histerese | ✅ concluído | `lib/rule_engine/*`, `include/thresholds.h` |
| M5 | Integração final UI/alertas e polimento operacional | 🚧 em andamento | integração ativa em `src/main.cpp` |
| M6 | Campanha de validação e consolidação de métricas | ⏳ pendente | usar `scripts/elm327_simulator.py` e plano em `docs/test_plan.md` |

## Observações

- A versão de build atual está em `PROJECT_VERSION="0.5.0"` (`platformio.ini`).
- O código já integra leitura OBD, avaliação por regras e renderização por severidade.
- A validação formal de desempenho/acurácia (M6) ainda não está fechada em relatório final.
