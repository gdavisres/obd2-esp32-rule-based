# Tabela de PIDs Monitorados

PIDs atualmente implementados no firmware (`include/obd_pids.h`, `lib/obd_client/obd_client.cpp`).

| PID | Nome | Unidade | Conversão (SAE J1979) | Uso principal |
|---|---|---|---|---|
| `0x00` | Supported PIDs 01-20 | bitmask | 4 bytes de máscara de suporte | handshake inicial e filtro de leitura |
| `0x05` | Engine Coolant Temperature (ECT) | °C | `A - 40` | termostato (P0128 / superaquecimento) |
| `0x06` | Short-Term Fuel Trim (STFT) | % | `(A - 128) * 100 / 128` | combustão (P0171/P0172) |
| `0x07` | Long-Term Fuel Trim (LTFT) | % | `(A - 128) * 100 / 128` | combustão (P0171/P0172) |
| `0x0C` | Engine RPM | rpm | `((A*256) + B) / 4` | regime do motor e classificação idle/high RPM |
| `0x0D` | Vehicle Speed | km/h | `A` | telemetria em tela normal |
| `0x0F` | Intake Air Temperature (IAT) | °C | `A - 40` | detecção de warm-up |
| `0x11` | Throttle Position | % | `A * 100 / 255` | telemetria em tela normal |
| `0x13` | O₂ Sensors Present | bitmask | leitura bruta | suporte aos sensores O₂ |
| `0x15` | O₂ Sensor 2 (downstream) | V e % trim | `V = A * 0.005`, `trim = (B - 128) * 100 / 128` | catalisador (P0420/P0430) |

## Estratégia de amostragem atual

- **1 Hz**: RPM, Speed, Throttle, STFT, LTFT
- **0.2 Hz (5 s)**: ECT, IAT
- **0.5 Hz (2 s)**: O₂ downstream

Referência de decisão: ADR-0005 (`docs/adr/0005-sampling-strategy.md`).
