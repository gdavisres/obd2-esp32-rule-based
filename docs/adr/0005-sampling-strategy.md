# ADR-0005: Estratégia de sampling de PIDs

* **Status**: accepted
* **Data**: 12/08/206
* **Decisor**: Vitor Fernandes da Silva

## Contexto

O ELM327 Bluetooth tem latência média de ~120 ms por request de PID. Para
ler os 9 PIDs do projeto em sequência, um ciclo completo demora ~1.1s,
limitando o sampling máximo prático a 1 Hz. A escolha da estratégia de
amostragem afeta diretamente:

1. Carga no canal Bluetooth e latência total do ciclo de leitura.
2. Atualização em tempo real de parâmetros de alta dinâmica vs. lentos.
3. Robustez contra requisições de PIDs não suportados pela ECU.
4. Disponibilidade de processamento para futuras tasks no ESP32.

As opções consideradas foram:

1. **Sampling uniforme 1 Hz para todos os PIDs**
2. **Sampling 2 Hz para PIDs rápidos**
3. **Mode 01 com múltiplos PIDs no mesmo request**
4. **Sampling diferenciado por categoria com cache de PIDs suportados**

## Decisão

Adotar **sampling diferenciado por categoria** combinado com **cache de PIDs suportados** (bitmask do PID 0x00).

## Justificativa

* **Sampling uniforme 1 Hz**: rejeitada porque 9 PIDs × 120 ms = 1080 ms, o
que ultrapassa o período de 1s e gera backlog de requisições.
* **Sampling 2 Hz para PIDs rápidos**: rejeitada porque o ELM327 V1.5
possui latência física de ~100 ms por request, tornando inviável atingir 500 ms.
* **Mode 01 com múltiplos PIDs no mesmo request**: rejeitada porque o ELM327
V1.5 não suporta essa funcionalidade nativamente sem comandos complexos
de baixo nível (ATCRA e múltiplos frames ISO-15765-4), fora do escopo da M3.
* **Sampling diferenciado com cache (escolhida)**: reduz a carga média para
~5 requests/s, mantendo a latência do ciclo em ~600-800 ms (abaixo do limite de 1s)
e evita mensagens `NO DATA` ao consultar PIDs não suportados.

## Parâmetros

Categorias de sampling:

* **Rápida (1 Hz)**: RPM, Speed, Throttle Position (dinâmica do motor muda rápido).
* **Média (1 Hz)**: STFT, LTFT (oscilam constantemente, mas não a cada ms).
* **Lenta (0.2 Hz / 5s)**: ECT, IAT (temperaturas mudam em escala de segundos).
* **Eventual (0.5 Hz / 2s)**: Sensores de O₂ (janela temporal para análise na FSM em M4).

Cache de bitmask:

* Populado no handshake (PID 0x00) e armazenado em `supported_bitmask[4]`.
* Apenas PIDs marcados como suportados são requisitados nos ciclos subsequentes.

## Consequências

Positivas:

* Latência por ciclo mantida bem abaixo de 1 segundo (~600-800 ms).
* Valores em tempo real atualizados no display a cada segundo.
* FSM de regras (M4) recebe snapshot com dados filtrados por suporte.
* Libera banda no canal Bluetooth para outras rotinas.

Negativas:

* PIDs lentos (ECT) só atualizam a cada 5s (aceitável para P0128/termostato).
* Veículos sem suporte ao PID 0x15 (O₂ sensor 2) impossibilitam a análise
de catalisador (P0420/P0430) em M4.

## Reavaliação

Reconsiderar se:

* Migramos para ELM327 V2.1 (suporta ATSP6 com frames múltiplos).
* Adicionamos comunicação CAN direta (sem passar pelo ELM327).
* Latência de 250 ms da meta de diagnóstico exigir sampling mais fino.

## Implementação

* `include/obd_pids.h`: estrutura `ObdSnapshot` com flags `_valid` e métricas do ciclo.
* `lib/obd_client/obd_client.cpp`:
* `supported_bitmask[4]` populado durante o estado `fsm_handshaking()`.
* `obd_client_read_all_supported_pids(snapshot)` gerencia intervalos via contador estático `cycle_counter`.



## Alternativas consideradas

* Sampling uniforme 1 Hz (rejeitada — ultrapassa 1s por ciclo).
* Sampling 2 Hz em PIDs rápidos (rejeitada — limitação física do hardware).
* Multi-PID request via Mode 01 (rejeitada — alta complexidade no ELM327 V1.5).