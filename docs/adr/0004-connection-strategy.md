# ADR-0004: Estratégia de reconexão — backoff exponencial

- **Status**: accepted
- **Data**: [preencher]
- **Decisor**: Gabriel Davi Silva Resende

## Contexto

Quando o ELM327 desconecta (Bluetooth fora de range, ELM327 desligado,
ECU desligada), o ESP32 precisa reconectar. A frequência das tentativas
afeta diretamente:

1. Tempo de recuperação após falhas transitórias.
2. Carga no canal Bluetooth (pode afetar outros dispositivos).
3. Consumo de energia (cada tentativa ativa o radio BT por ~5s).
4. Robustez contra falhas persistentes (ELM327 com defeito).

As opções consideradas foram:

1. **Reconexão imediata** (0s entre tentativas)
2. **Backoff fixo** (30s entre todas as tentativas)
3. **Backoff exponencial** (15s, 30s, 60s, 120s, cap 120s)
4. **Backoff exponencial com jitter** (acima + aleatório ±20%)

## Decisão

Adotar **backoff exponencial** com base 15s e cap em 120s, sem jitter.

## Justificativa

- **Reconexão imediata**: rejeitada porque pode piorar falhas do tipo
  "ELM327 reiniciando por brownout" — nova tentativa força novo reset.
- **Backoff fixo**: rejeitada porque é lenta para recuperar de falhas
  transitórias (ELM327 voltou em 5s, mas esperamos 30s).
- **Backoff exponencial com jitter**: mais robusta em sistemas com
  múltiplos clientes reconectando simultaneamente, mas adiciona
  complexidade sem benefício claro para um sistema com 1 cliente.
- **Backoff exponencial (escolhida)**: balanceia tempo de recuperação
  e carga no canal. As 4 primeiras tentativas (15s, 30s, 60s, 120s)
  cobrem cenários comuns (reboot do ELM327, queda de energia curta).

## Parâmetros

- Base: 15.000 ms (15s)
- Cap: 120.000 ms (2 min)
- Tentativas antes de logar "falha persistente": 5
- Não desiste: continua tentando indefinidamente após 5 tentativas

## Consequências

Positivas:
- Recupera de falhas transitórias em ~15s.
- Não satura o canal Bluetooth em falhas persistentes.
- Previsível (sem jitter) — facilita debug.

Negativas:
- Pode parecer "lento" em testes (após 5 falhas, espera 2min entre tentativas).
- Sem jitter: se múltiplos ESP32s em frotas reconectarem simultaneamente
  (após uma queda de energia coletiva), podem sincronizar e congestionar.
  Considerado aceitável para projeto acadêmico com 1 dispositivo.

## Reavaliação

Reconsiderar se:
- Projeto evoluir para frota de dispositivos.
- Latência de recuperação se tornar crítica (< 10s target).
- ELM327 demonstrar padrão de "reinicialização a cada N segundos" que
  encaixe melhor em backoff fixo.

## Implementação

- `include/obd_config.h`: `RECONNECT_BACKOFF_BASE_MS = 15000`,
  `RECONNECT_BACKOFF_CAP_MS = 120000`, `RECONNECT_MAX_ATTEMPTS = 5`.
- `lib/obd_client/obd_client.cpp`: função `calculate_backoff_delay(attempt)`.
- FSM transita CONNECTED → RECOVERING após falha; RECOVERING espera
  `calculate_backoff_delay(reconnect_attempts)` ms antes de voltar
  a DISCONNECTED (que reativa PAIRING).

## Alternativas consideradas

- Reconexão imediata (rejeitada — piora falhas transitórias).
- Backoff fixo 30s (rejeitada — lenta para falhas curtas).
- Backoff com jitter (rejeitada — complexidade sem benefício para 1 device).