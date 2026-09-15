# Glossario

| Sigla/Termo | Expansao | Descricao breve |
| --- | --- | --- |
| ADR | Architecture Decision Record | Documento que registra uma decisao arquitetural e sua justificativa. |
| CARB | California Air Resources Board | Orgao regulador veicular da California; referencia para OBD-II. |
| DTC | Diagnostic Trouble Code | Codigo de falha padronizado (ex.: P0128). |
| ECU | Engine Control Unit | Computador de bordo do veiculo. |
| ELM327 | - | Chip integrador OBD-II <-> UART/Bluetooth, da ELM Electronics. |
| enum class | - | Tipo enumerado com escopo forte em C++. |
| framebuffer | - | Area de memoria usada para desenhar a imagem antes de envia-la ao display. |
| GPIO | General Purpose Input/Output | Pino digital programavel do microcontrolador. |
| I2C | Inter-Integrated Circuit | Barramento serial sincrono, 2 fios (SDA + SCL). |
| I2C scanner | - | Sketch/utilitario para detectar dispositivos e enderecos no barramento I2C. |
| library.json | - | Manifesto da biblioteca no PlatformIO; descreve nome, versao e dependencias. |
| OBD-II | On-Board Diagnostics, 2a geracao | Padrao de diagnostico veicular obrigatorio nos EUA desde 1996. |
| page buffer | - | Buffer temporario organizado por paginas para renderizacao incremental no display. |
| PIC18F25K80 | - | Microcontrolador PIC da Microchip, usado no ELM327 V1.5 original. |
| PID | Parameter ID | Identificador de um parametro OBD-II (ex.: 0x05 = ECT). |
| SAE J1979 | - | Norma que define PIDs OBD-II. |
| SAE J2012 | - | Norma que define codigos DTC. |
| SPP | Serial Port Profile | Perfil Bluetooth que emula porta serial. |
| STFT | Short-Term Fuel Trim | Correcao de combustivel de curto prazo. |
| LTFT | Long-Term Fuel Trim | Correcao de combustivel de longo prazo (aprendida). |
| SSD1306 | - | Controlador de display OLED 128x64 com interface I2C. |
| mutex | - | Mecanismo de exclusao mutua usado para proteger recursos compartilhados. |
| thread-safety | - | Propriedade de um codigo poder ser chamado por multiplas tasks sem race conditions. |
| FSM | Finite State Machine | Maquina de estados finita. |
| Git | - | Sistema de controle de versao distribuido. |
| Branch | - | Linha de desenvolvimento independente no Git. |
| Commit | - | Snapshot de mudancas no codigo com mensagem descritiva. |
| PR | Pull Request | Pedido para mesclar mudancas de um branch para outro (usado no GitHub). |
| Conventional Commit | - | Convencao de formatacao de mensagens de commit (tipo: escopo: descricao). |
| GitFlow | - | Modelo de branching com branches feature, release, hotfix e main. |
| BT SPP | Bluetooth Serial Port Profile | Perfil Bluetooth que emula porta serial via RFCOMM. |
| ELM327 | — | Chip integrador OBD-II ↔ UART/Bluetooth da ELM Electronics. |
| PIC18F25K80 | — | Microcontrolador PIC usado no ELM327 V1.5 original. |
| AT command | — | Comando de configuração do ELM327 (ex.: ATZ, ATE0). |
| Backoff exponencial | — | Estratégia de retry onde o delay dobra a cada tentativa. |
| FSM | Finite State Machine | Máquina de estados finita usada para modelar a conexão. |
| Pairing | — | Processo de associação inicial entre dois dispositivos Bluetooth. |
| Prompt | — | Caractere > que o ELM327 envia para indicar pronto para próximo comando. |
| Task_LED | — | Tarefa FreeRTOS dedicada à execução de padrões de piscar do LED de forma não bloqueante. |
| Queue | Fila FreeRTOS | Estrutura thread-safe FIFO usada para envio de comandos de eventos entre tarefas. |
| Badge | — | Elemento gráfico retangular com cores invertidas no topo da tela para comunicar o estado rapidamente. |
| Refresh inteligente | — | Otimização que evita redesenhar o display quando a tela e o payload não sofreram alterações. |
| FNV-1a | Fowler–Noll–Vo hash | Algoritmo rápido de hashing utilizado para detectar alterações no payload de exibição. |