/**
 * @file main.cpp
 * @brief Ponto de entrada do firmware — Sistema de Diagnóstico OBD-II.
 *
 * Milestone M1: o main.cpp agora é fino. Toda a lógica de UI foi movida
 * para lib/display/. Aqui ficam apenas:
 *   - Inicialização (Serial + display_init).
 *   - Loop de validação que cicla pelas 3 telas implementadas em M1.
 *
 * Em M2+, este loop será substituído por tasks FreeRTOS.
 *
 * @author Gabriel Davi Silva Resende (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.1.0
 */

#include <Arduino.h>

#include "pinout.h"
#include "display.h"

// Versão do firmware, injetada via build_flags no platformio.ini.
static const char* FIRMWARE_VERSION = PROJECT_VERSION;

// Intervalo entre trocas de tela no loop de validação.
static const uint16_t SCREEN_CYCLE_MS = 3000;

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(100);  // Tempo para o CH340C estabilizar.

    Serial.println();
    Serial.println("========================================");
    Serial.println("OBD Diagnostic v" + String(FIRMWARE_VERSION));
    Serial.println("UNIFEI / PET-TEC / UNIGOU 2026");
    Serial.println("Fase: M1 — display driver + telas de status");
    Serial.println("========================================");

    // Configura LED onboard (continua piscando para heartbeat)
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    // Inicializa o display (fail-soft: continua se OLED não responder)
    bool ok = display_init();
    Serial.printf("[boot] display_init=%s\n", ok ? "OK" : "FAIL");

    if (ok) {
        // Renderiza tela de boot imediatamente
        DisplayPayload p = {};
        strncpy(p.line1, "OBD Diagnostic", sizeof(p.line1) - 1);
        strncpy(p.line2, FIRMWARE_VERSION,    sizeof(p.line2) - 1);
        strncpy(p.line3, "Iniciando...",       sizeof(p.line3) - 1);
        display_render(ScreenId::BOOT, p);
    }

    Serial.println();
}

// ============================================================================
// LOOP — cicla pelas 3 telas de M1 a cada SCREEN_CYCLE_MS para validação
// ============================================================================
void loop() {
    static uint8_t current_screen = 0;
    static uint32_t last_switch = 0;
    static uint32_t counter = 0;

    // Heartbeat no LED e no Serial
    digitalWrite(PIN_LED_STATUS, (millis() / 500) % 2);
    if (millis() - last_switch >= SCREEN_CYCLE_MS) {
        last_switch = millis();
        current_screen = (current_screen + 1) % 3;

        DisplayPayload p = {};
        switch (current_screen) {
            case 0:
                strncpy(p.line1, "OBD Diagnostic", sizeof(p.line1) - 1);
                strncpy(p.line2, FIRMWARE_VERSION, sizeof(p.line2) - 1);
                strncpy(p.line3, "Tela BOOT",      sizeof(p.line3) - 1);
                display_render(ScreenId::BOOT, p);
                Serial.printf("[cycle] tela=BOOT tick=%lu\n", static_cast<unsigned long>(counter++));
                break;
            case 1:
                strncpy(p.line1, "Aguardando",  sizeof(p.line1) - 1);
                strncpy(p.line2, "ELM327...",   sizeof(p.line2) - 1);
                strncpy(p.line3, "Conectando...", sizeof(p.line3) - 1);
                display_render(ScreenId::WAITING, p);
                Serial.printf("[cycle] tela=WAITING tick=%lu\n", static_cast<unsigned long>(counter++));
                break;
            case 2:
                strncpy(p.line1, "Sem conexao",    sizeof(p.line1) - 1);
                strncpy(p.line2, "Verifique ELM327", sizeof(p.line2) - 1);
                strncpy(p.line3, "Reset em 5s...",   sizeof(p.line3) - 1);
                display_render(ScreenId::NO_CONN, p);
                Serial.printf("[cycle] tela=NO_CONN tick=%lu\n", static_cast<unsigned long>(counter++));
                break;
        }
    }
}