# ADR-0002: Licença MIT vs GPL-3

**Status:** accepted  
**Data:** 12/07/2026  
**Decisor:** Gabriel Davi Silva Resende

## Contexto

O projeto de firmware IoT para diagnóstico OBD-II precisa escolher uma licença de código aberto. As opções viáveis eram:

- MIT: permissiva, permite uso comercial sem obrigação de compartilhar modificações.
- GPL-3: copyleft, obriga qualquer distribuição (incluindo modificações) a ser sob a mesma licença.
- Apache 2.0: permissiva com proteção contra patentes, menos popular em projetos embarcados.

## Decisão

Adotar licença **MIT**.

## Justificativa

- Simplicidade: MIT é a licença mais curta e clara, sem ambiguidades sobre "distribuição" no contexto de firmware embarcado.
- Compatibilidade: permite que a universidade (UNIFEI) ou terceiros integrem o código em produtos proprietários sem violações legais, ampliando potencial de adoção acadêmica e industrial.
- Realidade acadêmica: a maioria dos projetos IoT/embarcados open-source usa MIT ou Apache 2.0, não GPL. GPL-3 é mais comum em software desktop/servidor.
- Futuro: se o código evoluir para um componente reutilizável (ex.: "biblioteca OBD-II para ESP32"), MIT reduz barreiras para terceiros adotarem.

## Consequências

### Positivas

- Qualquer pessoa pode usar, modificar e vender o firmware sem pedir permissão, desde que incluam o aviso de MIT.
- Não há obrigação de publicar melhorias, reduzindo atrito para parceiros industriais.
- Alinhado com a tendência de IoT open-source moderno.

### Negativas

- Não há garantia de que melhorias externas sejam compartilhadas com o projeto.
- Se terceiros fizerem customizações valiosas, o projeto não herda automaticamente.

## Alternativas consideradas

- GPL-3: rejeitada porque imporia restrições desnecessárias em contexto acadêmico/industrial onde os objetivos são demonstração e educação, não forced sharing.
- Apache 2.0: rejeitada porque MIT oferece a mesma liberdade com documentação menor e menor reconhecimento legal desnecessário.

## Notas para futuro

Se o projeto envolver dependências GPL (ex.: uma biblioteca externa sob GPL-3), seria necessário reavaliação. Atualmente, as dependências previstas (SSD1306, BluetoothSerial, PubSubClient) são todas MIT ou Apache 2.0.

## Nota sobre formatação da linha de copyright

A linha de copyright foi simplificada para Copyright (c) 2026 Gabriel Davi Silva Resende (UNIFEI / PET-TEC) (uma única linha, com a atribuiçãoinstitucional entre parênteses) em vez de duas linhas separadas. Motivo:o detector de licença do GitHub (licensee) requer alta similaridade como template MIT canônico; múltiplas linhas Copyright (c) + texto extraderrubavam o score abaixo do limiar e o badge de "MIT License" nãoaparecia na página do repositório. A atribuição institucional à UNIFEIcontinua preservada, apenas em formato compactado.
