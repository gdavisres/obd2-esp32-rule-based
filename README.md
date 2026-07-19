# OBD-II ESP32 Rule-Based Diagnostic System

> Intelligent Vehicle Diagnostic System based on OBD-II with Rule-Based Fault
> Detection — rule-based baseline module for the UNIFEI / PET-TEC / UNIGOU 2026
> research project.

[![Platform](https://img.shields.io/badge/platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Framework](https://img.shields.io/badge/framework-Arduino--ESP32-teal.svg)](https://github.com/espressif/arduino-esp32)
[![Build Tool](https://img.shields.io/badge/build-PlatformIO-orange.svg)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](./LICENSE)
[![Language](https://img.shields.io/badge/language-C%2B%2B17-blue.svg)]()
[![Status](https://img.shields.io/badge/status-M1%20display%20driver-green.svg)]()

---

## 📖 Table of Contents

1. [Overview](#overview)
2. [Hardware](#hardware)
3. [Wiring](#wiring)
4. [Milestones](#milestones)
5. [Repository Structure](#repository-structure)
6. [Build & Flash](#build--flash)
7. [Development Workflow](#development-workflow)
8. [Documentation](#documentation)
9. [License](#license)
10. [References](#references)

---

## Overview

This repository contains the **rule-based baseline** firmware of an
on-board vehicle diagnostic system. The full research project targets
predictive fault detection via machine learning; this module establishes
the deterministic baseline against which the ML models will be evaluated.

M1 extracts the OLED driver into `lib/display/`, adds a thread-safe render
API with a FreeRTOS mutex, and keeps the firmware fail-soft if the display
is missing.

**Architecture (three layers):**

1. **Interface**: ELM327 Bluetooth adapter connected to the vehicle OBD-II
   port (J1962), speaking SAE J1979 over ISO 15765 CAN (11-bit, 500 kbps).
2. **Processing**: ESP32 receives PIDs via Bluetooth Classic SPP and runs
   a rule-based diagnostic engine with hysteresis and finite state machines.
3. **Output**: SSD1306 OLED 128x64 (I2C) for human-readable status +
   single LED with blink-pattern severity encoding.

**Monitored PIDs (SAE J1979):**

| PID  | Description              | Unit  |
|------|--------------------------|-------|
| 0x05 | Engine Coolant Temp      | °C    |
| 0x06 | Short-Term Fuel Trim     | %     |
| 0x07 | Long-Term Fuel Trim      | %     |
| 0x0C | Engine RPM               | rpm   |
| 0x0D | Vehicle Speed            | km/h  |
| 0x0F | Intake Air Temperature   | °C    |
| 0x11 | Throttle Position        | %     |
| 0x13 | O₂ Sensors Present       | bitmask |
| 0x15 | O₂ Sensor 2 (downstream) | V     |

**Target DTCs (SAE J2012):** P0128, P0171, P0172, P0420, P0430.

**Performance targets:** accuracy > 90%, false positive rate < 5%,
diagnostic latency < 250 ms per cycle.

---

## Hardware

| Component                | Model / Spec                       | Qty | Notes                              |
|--------------------------|------------------------------------|-----|------------------------------------|
| Microcontroller          | ESP32 DevKit 30-pin (CH340C)       | 1   | USB-UART bridge: WCH CH340C        |
| Display                  | OLED 0.96" 128x64 SSD1306 I2C      | 1   | Address 0x3C (or 0x3D — scan)      |
| OBD-II adapter           | ELM327 V1.5 (PIC18F25K80, original)| 1   | Bluetooth Classic SPP, PIN 1234    |
| LED                      | 5 mm LED (color: TBD per severity) | 1   | Red recommended for critical       |
| Resistor                 | 220 Ω 1/4 W                        | 1   | LED current limiter                |
| Breadboard               | 830-point                          | 1   |                                    |
| Jumper wires             | M-M and M-F                        | ~15 |                                    |
| USB cable                | USB-A to micro-USB (data-capable)  | 1   | **Must be data cable**             |

---

## Wiring

> Detailed schematic and pinout in [`docs/wiring.md`](./docs/wiring.md).

| ESP32 Pin | Component         | Component Pin | Notes                          |
|-----------|-------------------|---------------|--------------------------------|
| 3V3       | OLED SSD1306      | VCC           | **Never 5V** — will damage     |
| GND       | OLED SSD1306      | GND           | Common ground                  |
| GPIO22    | OLED SSD1306      | SCL           | I2C hardware SCL               |
| GPIO21    | OLED SSD1306      | SDA           | I2C hardware SDA               |
| GPIO2     | LED               | Anode (+)     | Via 220 Ω resistor             |
| GND       | LED               | Cathode (-)   |                                |

> **I2C pull-ups**: 4.7 kΩ–10 kΩ to 3V3 on SDA and SCL. Most OLED modules
> already have them onboard; verify with a multimeter or run an I2C scanner
> (see M1 for the scanner sketch).

---

## Milestones

| Milestone | Description                          | Status        | Docs                                  |
|-----------|--------------------------------------|---------------|---------------------------------------|
| Etapa 0   | PlatformIO + VS Code setup           | ✅ done        | —                                     |
| Etapa 1   | Git/GitHub repository                | ✅ done        | —                                     |
| M0        | Hello World + OLED boot + LED blink  | ✅ done        | —                                     |
| M1        | SSD1306 driver + status screens      | ✅ done        | [`docs/adr/0003-display-library-adafruit-vs-u8g2.md`](./docs/adr/0003-display-library-adafruit-vs-u8g2.md) |
| M2        | Bluetooth SPP + ELM327 V1.5 init     | 🚧 next        | —                                     |
| M3        | OBD-II PID layer + parser            | ⏳ pending     | —                                     |
| M4        | Rule engine + FSM + hysteresis       | ⏳ pending     | —                                     |
| M5        | OLED UI + LED blink patterns         | ⏳ pending     | —                                     |
| M6        | Validation campaign with simulator   | ⏳ pending     | —                                     |

Detailed status: [`docs/milestones.md`](./docs/milestones.md).

---

## Repository Structure

```
obd2-esp32-rule-based/
├── .github/           ← GitHub Actions workflows (added in M6)
├── docs/              ← Technical documentation (PT-BR, for PET-TEC report)
│   ├── adr/           ← Architecture Decision Records
│   ├── architecture.md
│   ├── decisions.md
│   ├── glossary.md
│   ├── milestones.md
│   ├── pid_table.md
│   ├── references.md
│   ├── roadmap.md
│   ├── test_plan.md
│   ├── test_results.md
│   ├── thresholds.md
│   └── wiring.md
├── include/           ← Shared headers (including pinout.h)
├── lib/               ← Local libraries (M1 starts with display/)
│   └── display/
├── scripts/           ← Python helper scripts (OBD simulator, log analyzer)
├── src/               ← Application entry point
│   └── main.cpp
├── test/              ← Unit tests (PlatformIO native env, from M4)
├── data/              ← LittleFS/SPIFFS files (unused in this phase)
├── .gitignore
├── CONTRIBUTING.md
├── LICENSE
├── platformio.ini
└── README.md
```

---

## Build & Flash

### Prerequisites

- [VS Code](https://code.visualstudio.com/) 1.85+
- [PlatformIO IDE](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) extension
- Python 3.10 or 3.11
- USB driver for CH340C:
  - Windows: [CH341SER.EXE](https://www.wch.cn/downloads/CH341SER_EXE.html)
  - Linux/macOS: built into kernel

### Build

```bash
pio run
```

Or use the ✓ Build button in the PlatformIO toolbar (bottom status bar).

### Flash

```bash
pio run -t upload
```

Or use the → Upload button. Make sure no other program is using the
serial port.

### Monitor

```bash
pio device monitor
```

Or use the 🔌 Serial Monitor button. Press Ctrl+C to exit.

### Build + Flash + Monitor in one command

```bash
pio run -t upload -t monitor
```

---

## Development Workflow

This project follows **GitFlow lite** with **Conventional Commits**.

### Branches

- `main` — stable, always builds, tagged at each milestone
- `dev` — current integration branch
- `feature/m<N>-<topic>` — one branch per milestone sub-task

### Commit message format

```
<type>(<scope>): <subject>

<body>

<footer>
```

Types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`, `build`, `ci`, `perf`.

Examples:
```
feat(m0): add platformio toolchain smoke test
fix(obd): handle ELM327 'NO DATA' response without crash
docs(adr): add ADR-0002 on FreeRTOS task priorities
refactor(rules): extract hysteresis into reusable template
```

See [`CONTRIBUTING.md`](./CONTRIBUTING.md) for full guidelines.

---

## Documentation

All technical documentation lives in [`docs/`](./docs/) and is written in
Brazilian Portuguese (target audience: PET-TEC report reviewers).

Key documents:

- [Architecture (three-layer diagram)](./docs/architecture.md)
- [PID table (Tab. 1 of the paper)](./docs/pid_table.md)
- [Thresholds with normative references](./docs/thresholds.md)
- [Wiring schematic and pinout](./docs/wiring.md)
- [Architecture Decision Records](./docs/adr/)
- [Glossary](./docs/glossary.md)
- [References](./docs/references.md)

---

## License

Released under the [MIT License](./LICENSE).

Copyright (c) 2026 Gabriel Davi Silva Resende, UNIFEI / PET-TEC.

---

## References

- SAE J1979 — E/E Diagnostic Test Modes
- SAE J2012 — Diagnostic Trouble Code Definitions
- CARB §1968.2 — Malfunction and Diagnostic System Requirements
- ELM327 Datasheet (ELM Electronics, v1.5)
- ESP32 Datasheet (Espressif Systems)
- SSD1306 Datasheet (Solaman Systech)

Full bibliography: [`docs/references.md`](./docs/references.md)