# Architecture Decision Records (ADRs)

Índice das decisões arquiteturais do projeto. Cada ADR fica em `docs/adr/NNNN-titulo.md`.

## Lista de ADRs

| # | Título | Status | Data |
|---|---|---|---|
| 1 | Escolha do toolchain (PlatformIO + Arduino-ESP32) | accepted | 12/07/2026 |
| 2 | Licença MIT vs GPL-3 | accepted | 12/07/2026 |
| 3 | Biblioteca de display (Adafruit_SSD1306 vs U8g2) | accepted | 19/07/2026 |
| 4 | Estratégia de reconexão com backoff exponencial | accepted | 07/08/2026 |
| 5 | Estratégia de sampling de PIDs | accepted | 12/08/2026 |
| 6 | Estratégia de histerese (N leituras consecutivas) | accepted | 18/08/2026 |

## Como criar um novo ADR

1. Copiar um ADR existente em `docs/adr/` como base.
2. Nomear como `NNNN-titulo-curto.md` com próximo número sequencial.
3. Preencher contexto, decisão, justificativa, consequências e alternativas.
4. Definir status inicial como `proposed`.
5. Após revisão técnica, atualizar para `accepted`.
6. Se substituído, marcar como `deprecated` e referenciar o ADR novo.

## Quando criar um ADR

Criar ADR para decisões com impacto estrutural, trade-offs relevantes ou alto potencial de reuso/discussão futura (por exemplo: estratégia de comunicação, modelagem de FSM, critérios de detecção).
