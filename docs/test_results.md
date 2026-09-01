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

## Template mínimo para consolidação futura

| Data | Versão firmware | Cenário | Resultado esperado | Resultado observado | Status |
|---|---|---|---|---|---|
| AAAA-MM-DD | v0.5.0+ | normal | OK / tela normal | _preencher_ | _preencher_ |
| AAAA-MM-DD | v0.5.0+ | p0128 | CRITICAL / P0128 | _preencher_ | _preencher_ |
| AAAA-MM-DD | v0.5.0+ | p0171_vac | CRITICAL / subtipo vacuum leak | _preencher_ | _preencher_ |
| AAAA-MM-DD | v0.5.0+ | p0171_inj | CRITICAL / subtipo injector | _preencher_ | _preencher_ |
| AAAA-MM-DD | v0.5.0+ | p0172 | CRITICAL / P0172 | _preencher_ | _preencher_ |
| AAAA-MM-DD | v0.5.0+ | p0420 | WARNING/CRITICAL catalisador | _preencher_ | _preencher_ |
| AAAA-MM-DD | v0.5.0+ | sem conexão | NO_CONN + reconexão | _preencher_ | _preencher_ |
