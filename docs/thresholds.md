
# Tabela de Thresholds de Diagnóstico (Tab. 2 do paper)

> Conforme CARB §1968.2, SAE J2012, SAE J1979.

## Termostato (PID 0x05) — P0128

| Condição | Estado | DTC |
|----------|--------|-----|
| ECT < 71°C após warm-up | CRITICAL | P0128 |
| 71 ≤ ECT ≤ 85°C após warm-up | WARNING | — |
| 86 ≤ ECT ≤ 89°C após warm-up | SUSPECT | — |
| 90 ≤ ECT ≤ 104°C | OK | — |
| ECT > 104°C | CRITICAL | P0217 (fora do escopo) |

### Detecção de warm-up

Condição OR:
- Tempo desde boot > 5 min, OU
- IAT > 30°C, OU
- ECT já atingiu ≥ 70°C desde boot

## Combustão (PIDs 0x06, 0x07) — P0171/P0172

| Condição | Estado | DTC |
|----------|--------|-----|
| STFT+LTFT > +15% (lean) | CRITICAL | P0171 |
| STFT+LTFT < -15% (rich) | CRITICAL | P0172 |
| LTFT > +20% | CRITICAL | P0171 |
| LTFT < -20% | CRITICAL | P0172 |
| STFT > +10% ou < -10% | WARNING | — |
| Caso contrário | OK | — |

### Diferenciação vazamento de vácuo vs. injetor

Aplicável quando combustão está em CRITICAL:

| Condição | Subtipo |
|----------|---------|
| STFT@idle - STFT@high_RPM > 10% | Vazamento de vácuo |
| STFT@idle - STFT@high_RPM ≤ 10% | Falha de injetor |
| Sem amostras high-RPM ainda | Indeterminate |

Onde:
- `idle`: RPM < 1000
- `high_RPM`: RPM > 2500
- `STFT@idle` / `STFT@high_RPM`: média das últimas 8 amostras de cada categoria

## Catalisador (PID 0x15) — P0420/P0430

| Condição | Estado | DTC |
|----------|--------|-----|
| > 5 transições do O₂ downstream em 10s | CRITICAL | P0420 |
| > 2 transições em 10s | WARNING | — |
| Caso contrário | OK | — |

Onde:
- **Transição**: |V_atual - V_anterior| > 0.4 V
- **Janela**: 10 s rolling
- **Sampling**: 0.5 Hz (limitação Bluetooth — ver ADR-0005)

### Limitação importante

O ELM327 Bluetooth limita sampling de O₂ a ~0.5 Hz, dando ~5 amostras
em 10 s. Sensibilidade reduzida para oscilações de ~1 Hz características
de catalisador degradado. Para M6, considerar ELM327 WiFi ou CAN direto.

## Histerese

| Parâmetro | Valor | Justificativa |
|-----------|-------|---------------|
| Confirmações para transitar | 3 ciclos (3 s @ 1 Hz) | Filtra flutuações transitórias sem atrasar falhas reais |
| Reset por timeout | 30 s sem leitura | Evita counter "preso" durante reconexão BT |
| Detecção de flapping | OK→WARN→OK em < 5 s | Métrica de FPR |

## Referências normativas

- CARB §1968.2 — Malfunction and Diagnostic System Requirements.
- SAE J2012 — Diagnostic Trouble Code Definitions.
- SAE J1979 — E/E Diagnostic Test Modes (PIDs e fórmulas).
- ISO 15031-5 — Road vehicles diagnostics.
