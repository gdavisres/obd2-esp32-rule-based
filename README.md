# OBD-II ESP32 Rule-Based Diagnostic System

> Rule-based baseline firmware for OBD-II fault detection on ESP32.

[![Platform](https://img.shields.io/badge/platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Framework](https://img.shields.io/badge/framework-Arduino--ESP32-teal.svg)](https://github.com/espressif/arduino-esp32)
[![Build Tool](https://img.shields.io/badge/build-PlatformIO-orange.svg)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](./LICENSE)
[![Language](https://img.shields.io/badge/language-C%2B%2B17-blue.svg)]()
[![Status](https://img.shields.io/badge/status-M4%20core%20complete%20%7C%20M5%20integration%20ongoing-green.svg)]()

---

## Overview

This repository contains the current **rule-based baseline** for vehicle diagnosis.

Implemented in firmware today:

- ESP32 + PlatformIO + Arduino toolchain
- OLED SSD1306 display subsystem (`lib/display`)
- Bluetooth SPP OBD client for ELM327 (`lib/obd_client`)
- PID conversion + snapshot aggregation for 9 monitored PIDs
- Rule engine with FSM + hysteresis for thermostat, combustion, and catalytic diagnostics (`lib/rule_engine`)
- Severity-driven UI and LED behavior integrated in `src/main.cpp`
- ELM327 simulator script for validation scenarios (`scripts/elm327_simulator.py`)

Target DTCs: **P0128, P0171, P0172, P0420, P0430**.

---

## Current Milestone Status

| Milestone | Description | Status |
|---|---|---|
| Etapa 0 | PlatformIO + VS Code setup | ✅ done |
| Etapa 1 | Git/GitHub repository setup | ✅ done |
| M0 | Boot firmware + OLED/LED smoke | ✅ done |
| M1 | Display driver + base status screens | ✅ done |
| M2 | Bluetooth SPP + ELM327 initialization FSM | ✅ done |
| M3 | OBD-II PID layer + parser + sampling strategy | ✅ done |
| M4 | Rule engine + FSM + hysteresis | ✅ done |
| M5 | UI/alerts refinement + integrated behavior polish | 🚧 in progress |
| M6 | Validation campaign + metrics consolidation | ⏳ pending |

Detailed tracking: [`docs/milestones.md`](./docs/milestones.md).

---

## Repository Structure

```text
obd2-esp32-rule-based/
├── docs/
│   ├── adr/
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
├── include/
│   ├── obd_config.h
│   ├── obd_pids.h
│   ├── pinout.h
│   └── thresholds.h
├── lib/
│   ├── display/
│   ├── obd_client/
│   └── rule_engine/
├── scripts/
│   └── elm327_simulator.py
├── src/
│   └── main.cpp
├── test/
├── CONTRIBUTING.md
├── LICENSE
├── platformio.ini
└── README.md
```

---

## Build, Flash, Monitor

```bash
pio run
pio run -t upload
pio device monitor
```

Combined:

```bash
pio run -t upload -t monitor
```

---

## Documentation

Project documentation is in [`docs/`](./docs/), primarily in PT-BR.

Key docs:

- [Architecture](./docs/architecture.md)
- [PID table](./docs/pid_table.md)
- [Thresholds](./docs/thresholds.md)
- [Wiring](./docs/wiring.md)
- [Milestones](./docs/milestones.md)
- [ADRs index](./docs/decisions.md)
- [Test plan](./docs/test_plan.md)
- [Test results](./docs/test_results.md)

---

## License

Released under the [MIT License](./LICENSE).
