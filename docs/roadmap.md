═══════════════════════════════════════════════════════════════════════════════
  ROADMAP — Sistema de Diagnóstico Veicular OBD-II com Regras (UNIFEI/PET-TEC)
  Fase atual: MÓDULO BASEADO EM REGRAS (sem ML/TFLM — fase futura)
═══════════════════════════════════════════════════════════════════════════════

  ETAPA 0 (setup)        M0          M1          M2          M3          M4          M5          M6
  ─────────────          ───         ───         ───         ───         ───         ───         ───
  Toolchain + roadmap ►  Hello      Driver      Bluetooth   Camada      Motor de    Interface   Validação
  PlatformIO + VS Code   World       SSD1306     SPP +       OBD-II      regras +    OLED +      com
                         + boot      + telas     ELM327      + PIDs +    FSM +       padrões     simulador
                         no OLED     de status   V1.5        parser +    histerese   de LED      (casos de
                         + LED       (boot,      (PIC18F25   simulador  (3 FSMs:    (4 telas +  teste P0128,
                         blink       waiting,    K80, init   Python     termostato, 4 padrões   P0171, P0172,
                         + log       no conn)    ATZ/ATE0..  ou Torque  combustão,  de piscar)  P0420, P0430)
                         Serial                  ATSP0)      Lite)      catalisador)             + normal
                                                                                                  + sem conn

  Dependências:    M0 ─► M1 ─► M2 ─► M3 ─► M4 ─► M5 ─► M6
                    │     │      │      │      │      │
                    └─────┴──────┴──────┴──────┴──────┴── cada uma depende da anterior estar ACEITA

  Estimativa de tempo (horas focadas, sem overhead):
  ┌─────────┬──────────────┬─────────────────────────────────────────────────────┐
  │ Milest. │ Horas (range)│ Justificativa                                       │
  ├─────────┼──────────────┼─────────────────────────────────────────────────────┤
  │ M0      │  2 – 4 h     │ Setup já feito em Etapa 0; só validar blink+OLED    │
  │ M1      │  4 – 6 h     │ Biblioteca SSD1306 é madura; trabalho é na FSM tela │
  │ M2      │  8 – 12 h    │ Stack BT SPP tem pegadinhas; AT commands a debugar  │
  │ M3      │  8 – 10 h    │ Parser de PIDs é mecânico; simulador toma 2–3 h     │
  │ M4      │ 12 – 16 h    │ FSM + histerese é a parte intelectualmente densa    │
  │ M5      │  6 – 8 h     │ UI é "cosmético" mas exige cuidado com threading    │
  │ M6      │  8 – 10 h    │ Campanha de testes + relatório de resultados        │
  ├─────────┼──────────────┼─────────────────────────────────────────────────────┤
  │ TOTAL   │ 48 – 66 h    │ Trabalho focado                                     │
  │ + 30%   │ +14 – 20 h   │ Overhead realista (debug, retrabalho, orientação)   │
  │ = REAL  │ 62 – 86 h    │ Em ritmo acadêmico de 10–15 h/semana → 5–7 semanas  │
  └─────────┴──────────────┴─────────────────────────────────────────────────────┘

  Marcos de aceite (GO/NO-GO):
  • M0 → toolchain validado, OLED e LED respondendo
  • M1 → 3 telas (boot/waiting/no_conn) renderizando thread-safely
  • M2 → "41 00 BE 3FA8 13" recebido em resposta a "0100"
  • M3 → ECT, RPM, STFT, LTFT lidos a 1 Hz com parsing robusto
  • M4 → FSM transita OK→SUSPECT→WARNING→CRITICAL com histerese de 3 ciclos
  • M5 → 4 telas + 4 padrões de LED integrados, latência < 250 ms
  • M6 → tabela de 7 casos (5 DTCs + normal + sem conn) ≥ 90% acurácia, < 5% FPR
═══════════════════════════════════════════════════════════════════════════════