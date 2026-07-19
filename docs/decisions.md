# Architecture Decision Records (ADRs)

Índice de decisões arquiteturais do projeto. Cada ADR é um arquivo `emocs/adr/NNNN-titulo.md` com a estrutura:
contexto, decisão, justificativa, consequências, alternativas, status.

## Lista de ADRs

| # | Título                                                        | Status   | Data       |
|---|---------------------------------------------------------------|----------|------------|
| 1 | Escolha do toolchain (PlatformIO+Arduino)                     | accepted | 12/07/2026 |
| 2 | Licença MIT vs GPL-3                                          | accepted | 12/07/2026 |
| 3 | Escolha da biblioteca de display (Adafruit_SSD1306 vs U8g2)   | accepted | 19/07/2026 |

## Como criar um novo ADR

1. Copie o template de `docs/adr/0001-toolchain-platformio.md`.
2. Nomeie como `NNNN-titulo-curto.md` (próximo número sequencial).
3. Preencha contexto, decisão, justificativa, consequências, alternativas.
4. Status inicial: `proposed`.
5. Após revisão/orientação: mude para `accepted`.
6. Se for substituída no futuro: marque como `deprecated` e crie um novo ADR referenciando o anterior.

## Quando criar um ADR

Crie um ADR sempre que tomar uma decisão que:

- Afete a estrutura do firmware (ex.: escolha de biblioteca, padrão decomunicação entre tasks, estratégia de FSM).
- Tenha trade-offs não-triviais (ex.: performance vs. consumo, histerrese vs. média móvel).
- Possa ser questionada no futuro (ex.: "por que MIT e não GPL-3?").
- Seja referenciada no relatório PET-TEC.

NÃO crie ADR para decisões triviais (ex.: nome de variável, cor de LED).
