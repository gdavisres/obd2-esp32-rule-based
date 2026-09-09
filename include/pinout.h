/**
 * @file pinout.h
 * @brief Mapeamento central de pinos do projeto OBD-II.
 *
 * Centralizar os pinos em um único arquivo é boa prática porque:
 *   - Permite trocar o hardware (ex.: passar de ESP32 para ESP32-S3)
 *     modificando apenas este arquivo.
 *   - Evita "números mágicos" espalhados pelo código.
 *   - Documenta o uso de cada GPIO em um só lugar.
 *
 * Convenção: PIN_<SUBSISTEMA>_<FUNÇÃO>. Ex.: PIN_I2C_SDA, PIN_LED_STATUS.
 *
 * @author Gabriel Davi Silva Resende (PET-TEC UNIFEI)
 * @author Rafael (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.1.0
 */

#pragma once

#include <cstdint>

// ============================================================================
// I2C (barramento para OLED SSD1306)
// ============================================================================
// ESP32 DevKit 30 pinos tem I2C hardware mapeado em GPIO21 (SDA) e GPIO22 (SCL).
// Estes são os pinos padrão da Wire.h no core Arduino-ESP32 — não mudar sem
// justificativa técnica.
static const uint8_t PIN_I2C_SDA = 21;
static const uint8_t PIN_I2C_SCL = 22;

// Frequência do barramento I2C. 400 kHz é "Fast-mode" — suficiente para
// OLED sem flicker. Reduzir para 100 kHz se houver instabilidade com cabos longos.
static const uint32_t I2C_FREQ_HZ = 400000;

// ============================================================================
// Display OLED SSD1306 128x64
// ============================================================================
static const uint8_t  SCREEN_WIDTH  = 128;
static const uint8_t  SCREEN_HEIGHT = 64;

// Endereço I2C do SSD1306. A maioria dos módulos vem configurado em 0x3C.
// Alguns vêm em 0x3D — usar o I2C scanner (Passo 3.9) para descobrir.
static const uint8_t  OLED_ADDRESS = 0x3C;

// Pino de reset do OLED. -1 significa "sem pino de reset dedicado" (usar
// o reset do próprio ESP32). A Adafruit_SSD1306 aceita -1 como convenção.
static const int8_t   OLED_RESET   = -1;

// ============================================================================
// LED indicador de status
// ============================================================================
// GPIO2 é o LED onboard azul do ESP32 DevKit. Para flexibilidade futura
// (LED externo em protoboard), pode ser remapeado.
static const uint8_t  PIN_LED_STATUS = 2;

// ============================================================================
// Pinos reservados para futuras milestones (não usar em M1)
// ============================================================================
// M5+: botão físico para alternar entre telas
// static const uint8_t PIN_BTN_CYCLE = 14;

// M2+: caso queiramos adicionar um buzzer para alarmes sonoros críticos
// static const uint8_t PIN_BUZZER    = 15;