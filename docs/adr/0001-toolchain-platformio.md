# ADR-0001: Escolha do toolchain (PlatformIO + Arduino-ESP32)

**Status:** accepted  
**Data:** 12/07/2026
**Decisor:** Gabriel Davi Silva Resende

## Contexto

O projeto precisa de um toolchain para compilar firmware para o ESP32 DevKit 30 pinos (CH340C). As opções viáveis eram:

- Arduino IDE: oficialmente suportada pelo core Arduino-ESP32.
- PlatformIO IDE (no VS Code): wrapper sobre o core Arduino-ESP32 com gerenciamento de dependências, estrutura de projeto e CI.
- ESP-IDF puro: framework oficial da Espressif, mais baixo nível.

## Decisão

Adotar PlatformIO IDE no VS Code com `framework = arduino`.

## Justificativa

- Gerenciamento de dependências declarativo via `lib_deps` no `platformio.ini`, reprodutível entre máquinas da UNIFEI.
- Estrutura de projeto forçada (`src/`, `lib/`, `include/`, `test/`, `data/`), alinhada à modularidade exigida em M3 (uma biblioteca por subsistema).
- Integração com VS Code: IntelliSense semântico, debug JTAG (futuro), terminal integrado e Git nativo.
- CI em linha de comando: `pio run` é scriptável para GitHub Actions.
- Curva de aprendizado mais suave que ESP-IDF puro, suficiente para a fase baseada em regras. Reavaliável na fase ML.

## Consequências

### Positivas

- Reprodutibilidade entre máquinas (lock de versões em `platformio.ini`).
- Estrutura pronta para crescer modularmente em M1-M6.
- Documentação oficial ampla e ativa.

### Negativas

- Dependência de uma ferramenta de terceiros (PlatformIO) em cima do core Arduino-ESP32. Se o PlatformIO descontinuar, migração para ESP-IDF puro é possível, mas trabalhosa.
- `lib_deps` com versão `^x.y.z` ainda permite minor updates; se a reprodutibilidade absoluta for necessária (ex.: para o paper), travar com `@x.y.z` exato.

## Alternativas consideradas

- Arduino IDE: rejeitada por não ter lock de versões nem estrutura de projeto modular.
- ESP-IDF puro: rejeitado nesta fase pela curva de aprendizado e porque as bibliotecas SSD1306 e BluetoothSerial do core Arduino simplificam M1 e M2. Pode ser reconsiderado na fase ML.