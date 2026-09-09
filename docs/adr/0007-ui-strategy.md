# ADR-0007: Estratégia de UI — densidade vs. clareza

- **Status**: accepted
- **Data**: 09/09/2026
- **Decisor**: Rafael

## Contexto

Display OLED 128x64 monocromático é pequeno. Precisamos decidir
quanta informação mostrar por tela.

## Decisão

Adotar **hierarquia visual com 4 linhas por tela** + **badges de status**.

## Justificativa

### Hierarquia visual

| Elemento | Fonte | Quando usar |
|----------|-------|-------------|
| DTC code (P0128) | 2 (16px) | Apenas em CRITICAL |
| Badge de status | 1 (8px) invertido | Topo de cada tela |
| Dados live | 1 (8px) | 3-4 linhas no NORMAL |
| Footer | 1 (8px) | Hint/timestamp |

### Badges de status

Pequenos retângulos invertidos (texto preto em fundo branco) no topo
de cada tela comunicam instantaneamente o estado:
- "OK" (NORMAL)
- "! AVISO" (WARNING)
- "AGUARDANDO" (WAITING)

### Animações sutis

- WAITING: 3 retângulos pulsantes (300 ms por passo).
- WARNING: "!" pulsante à direita (1 Hz).
- CRITICAL: borda piscante (2 Hz).

Animações comunicam urgência sem serem agressivas.

## Consequências

Positivas:
- Display claro mesmo a 1 m de distância.
- Reconhecimento de severidade sem ler texto.
- Animações chamam atenção mas não distraem.

Negativas:
- 4 linhas limitam densidade de informação.
- Badges ocupam 12 px verticais (18% do display).
- Animações exigem re-render even quando conteúdo não muda.

## Alternativas consideradas

- **Max density (6+ linhas)**: rejeitado — texto fica ilegível.
- **Single big number**: rejeitado — não mostra contexto.
- **Graphs**: rejeitado — complexo de implementar em monocromático.

## Reavaliação

Reconsiderar se:
- Display maior for migrado (ex.: 2.42" 128x64).
- Cores forem adicionadas (OLED colorido).
- Múltiplos PIDs críticos precisarem ser mostrados simultaneamente.

## Implementação

- `lib/display/display.cpp` com render_*() refinado por tela.
- `compute_hash()` para refresh inteligente.
- Animações baseadas em `millis()`, non-blocking.
