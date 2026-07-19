# ADR-0003: Escolha da biblioteca de display (Adafruit_SSD1306 vs U8g2)

- **Status**: accepted
- **Data**: 19/07/2026
- **Decisor**: Gabriel Davi Silva Resende

## Contexto

O projeto precisa de uma biblioteca para abstrair o controlador SSD1306 do
display OLED 128x64 via I2C. As duas opções viáveis e amplamente adotadas
na comunidade Arduino-ESP32 são:

1. **Adafruit_SSD1306 + Adafruit_GFX** — biblioteca oficial da Adafruit,
   API simples baseada em framebuffer completo.
2. **U8g2** — biblioteca universal de Oliver Kraus, com três modos de
   buffer (full, page, u8x8) e centenas de fontes.

## Decisão

Adotar **Adafruit_SSD1306 + Adafruit_GFX**.

## Justificativa

A análise de trade-offs (documentada em `docs/adr/0003-display-library-adafruit-vs-u8g2.md`)
favora Adafruit pelos seguintes motivos específicos ao projeto:

1. **Escopo de UI limitado**: 5 telas previstas (boot, waiting, no_conn,
   normal, warning/critical) são todas texto + poucos elementos gráficos.
   Adafruit cobre trivialmente.
2. **RAM não é restrição**: 1 KB de framebuffer em 520 KB de SRAM do ESP32
   é 0.2%. Page buffer do U8g2 traria economia irrelevante.
3. **Fase ML futura**: TensorFlow Lite Micro compete por RAM, mas o
   gargalo serão os pesos do modelo (20-80 KB), não o framebuffer.
4. **Curva de aprendizado**: API consistente com Serial.print() do Arduino.
5. **Documentação vasta**: tutoriais abundantes para iniciantes.

## Consequências

Positivas:
- API simples, código legível para o relatório PET-TEC.
- Footprint de código pequeno (~6 KB flash).
- Manutenção do idiomatismo `display.print()` herdado do M0.

Negativas:
- Sem atualização parcial — cada `display.display()` envia 1024 bytes
  via I2C (~25 ms @ 400 kHz). Em UIs estáticas, é desperdício aceitável.
- Sem fontes especiais (CJK, anti-aliasing). Não necessário para M1-M5.
- Sem page buffer mode. Não necessário dada a RAM disponível.

## Alternativas consideradas

- **U8g2**: rejeitada pelos motivos acima, mas seria a escolha correta
  se houvesse necessidade de fontes especiais, UI densa com animação,
  ou se RAM fosse um recurso crítico (ex.: em microcontrolador AVR).

## Reavaliação

Esta decisão deve ser revisitada se, em M5, ocorrer qualquer uma destas
condições:

- UI ficar densa a ponto de `display.display()` causar flicker perceptível.
- Necessidade de fontes especiais (CJK, grandes, anti-aliasing).
- Latência de refresh do display impactar o requisito de latência de
  diagnóstico < 250 ms.

Nesses casos, migrar para U8g2. Como a UI está isolada em `lib/display/`,
a migração toca apenas essa biblioteca — o resto do firmware não é afetado.

## Implementação

- `lib_deps` no `platformio.ini`:
  - `adafruit/Adafruit SSD1306 @ ^2.5.7`
  - `adafruit/Adafruit GFX Library @ ^1.11.9`
- `lib/display/display.cpp` faz `#include <Adafruit_SSD1306.h>`.
- A instância `Adafruit_SSD1306 display(...)` é estática (sem `new`).
- Mutex FreeRTOS (`xSemaphoreCreateMutex`) serializa `display_render`.