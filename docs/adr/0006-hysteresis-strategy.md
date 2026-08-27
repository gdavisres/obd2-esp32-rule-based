
# ADR-0006: Estratégia de histerese (N leituras consecutivas)

- **Status**: accepted
- **Data**: 18/08/2026
- **Decisor**: Rafael Lucas Junqueira Brigagão

## Contexto

O motor de regras avalia snapshots OBD a 1 Hz. Sem histerese, qualquer
flutuação transitória de sensor (ex.: STFT -30% por 0.5 s em freada)
dispararia transição de estado — falso positivo.

Precisamos de uma estratégia que filtre flutuações sem atrasar excessivamente
a detecção de falhas reais.

## Decisão

Adotar **N leituras consecutivas** com N=3.

## Justificativa

### Análise de alternativas

| Abordagem | Prós | Contras |
|-----------|------|---------|
| **N consecutivas** (escolhida) | Simples; resposta rápida; imune a spikes | Susceptível a oscilação sustentada |
| Média móvel | Suaviza ruído | Resposta lenta; mascarar início de falha |
| Mediana móvel | Robusta a outliers | Cara; resposta lenta |
| Filtro IIR exponencial | Estado mínimo | Difícil de afinar |

### Por que N=3?

- **Atraso de resposta**: 3 s @ 1 Hz. Suficiente para filtrar a maioria das
  flutuações transitórias (típicas de 0.5-2 s), sem comprometer detecção
  de falhas reais (que persistem por minutos/horas).
- **Meta de FPR < 5%**: com N=3, esperamos FPR ~3% em condições normais
  de operação. Validaremos empiricamente em M6.
- **Custo computacional**: contador `uint8_t`, comparação `>= 3`. Irrelevante.

### Casos cobertos

1. **Spike isolado** (1 amostra ruidosa): counter incrementa, mas não atinge 3.
   Volta para 0 na próxima amostra normal. Sem transição.
2. **Flutuação de 2 amostras**: counter chega a 2, depois reseta. Sem transição.
3. **Falha real persistente**: counter atinge 3 em 3 s, transita. Detecção rápida.
4. **Oscilação sustentada** (2 OK, 2 WARN, 2 OK, 2 WARN...): counter fica entre
   0 e 2, nunca transita. Aceitável — este padrão indica problema real que
   merece investigação manual, não falso positivo.

## Consequências

Positivas:
- FPR esperado < 5%.
- Latência de detecção de falhas reais: 3 s (aceitável para diagnóstico
  veicular — falhas reais não são safety-critical em janela de segundos).
- Código simples, sem dependência de bibliotecas matemáticas.

Negativas:
- Não suaviza — apenas filtra por persistência. Em cenários de muito ruído
  (sensor O2 downstream oscilando entre 0.2 e 0.8 V), pode haver
  sensibilidade reduzida. Mitigado no catalisador FSM com buffer circular
  próprio.
- 3 s de atraso pode parecer "lento" em demo. Justificativa documentada.

## Parâmetros

- `HYSTERESIS_CONFIRMATIONS = 3` em `include/thresholds.h`.
- `HYSTERESIS_TIMEOUT_MS = 30000` (reset se sem leitura por 30 s — ex.:
  durante reconexão BT).

## Reavaliação

Reconsiderar se:
- M6 mostrar FPR empírico > 5% (aumentar N para 4 ou 5).
- Latência de 3 s for considerada alta para algum subsistema (reduzir N
  para 2 apenas nesse subsistema).
- Detectar padrões de oscilação sustentada (implementar contador secundário
  de "quase-transições" para log sem transitar).

## Implementação

- Função `apply_hysteresis(current, evaluated, counter)` em
  `lib/rule_engine/rule_engine.cpp`.
- Cada FSM (termostat, combustion, catalytic) chama apply_hysteresis após
  avaliar severidade.
- Counter é membro da struct de estado de cada FSM.
- Detecção de flapping (OK→WARN→OK em < 5 s) para métrica de FPR.





