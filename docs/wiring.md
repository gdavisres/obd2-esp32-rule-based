# Esquema Elétrico e Pinout

> Atualizado em M1. Diagrama esquemático completo será incluído em M2
> (após validação do Bluetooth).

## Pinout do ESP32 DevKit 30 pinos (CH340C)

| GPIO  | Função no projeto       | Componente          | Observações                  |
|-------|-------------------------|---------------------|------------------------------|
| GPIO2 | LED status              | LED onboard azul    | Saída digital, 1 Hz heartbeat |
| GPIO21| I2C SDA                 | OLED SSD1306 SDA    | Hardware I2C do ESP32        |
| GPIO22| I2C SCL                 | OLED SSD1306 SCL    | Hardware I2C do ESP32        |
| 3V3   | Alimentação 3.3V        | OLED VCC            | **NUNCA 5V** — queima o OLED |
| GND   | Referência comum        | OLED GND + LED cat. | Comum a todos os componentes |

> Pinos reservados para futuras milestones:
> - GPIO14 (botão de cycle) — M5+
> - GPIO15 (buzzer opcional) — decisão: usar LED em vez de buzzer

## Esquemático ASCII (M1)

```
                 ESP32 DevKit 30 pinos
                 ┌─────────────────────┐
                 │                     │
   USB (CH340C) ─┤                     ├─ 3V3 ────┐
                 │                     ├─ GND ──┐ │
                 │                     │        │ │
                 │   GPIO2 ────────────┤        │ │
                 │                     │        │ │
                 │   GPIO21 (SDA) ─────┤        │ │
                 │                     │        │ │
                 │   GPIO22 (SCL) ─────┤        │ │
                 │                     │        │ │
                 └─────────────────────┘        │ │
                                                │ │
                       ┌────────────────────────┘ │
                       │                          │
                       │     LED                  │
                       │    ─┬─                   │
                       │     │                    │
                       │    ─┴─  220 Ω            │
                       │     │                    │
                       │     └────────────────────┘
                       │
                       │   OLED SSD1306 128x64
                       │   ┌──────────────────┐
                       │   │                  │
                       └───┤ VCC              │
                           │                  │
                       ┌───┤ GND              │
                       │   │                  │
                       │   │ SDA ←── GPIO21   │
                       │   │                  │
                       │   │ SCL ←── GPIO22   │
                       │   │                  │
                       │   └──────────────────┘
                       │
                       └── Pull-ups 4.7 kΩ para 3V3 (geralmente onboard no módulo OLED)
```

## Cálculo do resistor do LED

V_led = 2.0 V (típico LED vermelho/azul)
I_led = 12 mA (visível confortável, abaixo do limite de 40 mA do GPIO)
R = (Vcc - V_led) / I_led = (3.3 - 2.0) / 0.012 = 108 Ω

Usar 220 Ω (padrão comercial, 5.5 mA — ainda visível e mais seguro).
Aceitável até 330 Ω (3.9 mA — visível em ambiente indoor).

## Pull-ups do I2C

A maioria dos módulos OLED SSD1306 já tem pull-ups de 4.7 kΩ ou 10 kΩ
onboard. Verificar com multímetro (resistência entre SDA e VCC).
Se não houver, adicionar 4.7 kΩ entre SDA-VCC e 4.7 kΩ entre SCL-VCC.
Sem pull-ups, I2C não funciona (linhas ficam flutuantes).

## Validação

- Antes de ligar: checar continuidade GND com multímetro (todos os
  componentes devem ter GND comum).
- Ao ligar: OLED deve mostrar pixels aleatórios por <1 s (power-on
  reset do SSD1306) e depois ficar preto até display_init() rodar.
- Se OLED não mostrar nada após display_init(): rodar I2C scanner
  (Passo 3.9) para confirmar endereço.