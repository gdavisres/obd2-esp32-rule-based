/**
 * @file main.cpp
 * @brief Ponto de entrada do firmware — Sistema de Diagnóstico OBD-II.
 *
 * Milestone M2: integração do subsistema OBD client (Bluetooth SPP + ELM327).
 * O loop() agora chama obd_client_loop() para gerenciar a FSM de conexão.
 * O display reage ao estado da FSM: WAITING em PAIRING/INITIALIZING,
 * NO_CONN em RECOVERING/DISCONNECTED.
 *
 * @author Gabriel Davi Silva Resende (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.2.0
 */

#include <Arduino.h>

#include "pinout.h"
#include "display.h"
#include "obd_client.h"

static const char* FIRMWARE_VERSION = PROJECT_VERSION;

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println();
    Serial.println("========================================");
    Serial.println("OBD Diagnostic v" + String(FIRMWARE_VERSION));
    Serial.println("UNIFEI / PET-TEC / UNIGOU 2026");
    Serial.println("Fase: M2 — Bluetooth SPP + ELM327 V1.5");
    Serial.println("========================================");

    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    bool disp_ok = display_init();
    Serial.printf("[boot] display_init=%s\n", disp_ok ? "OK" : "FAIL");

    // Tela de boot
    if (disp_ok) {
        DisplayPayload p = {};
        strncpy(p.line1, "OBD Diagnostic", sizeof(p.line1) - 1);
        strncpy(p.line2, FIRMWARE_VERSION, sizeof(p.line2) - 1);
        strncpy(p.line3, "Boot M2...",     sizeof(p.line3) - 1);
        display_render(ScreenId::BOOT, p);
    }

    // Inicializa cliente OBD (Bluetooth SPP)
    bool obd_ok = obd_client_init();
    Serial.printf("[boot] obd_client_init=%s\n", obd_ok ? "OK" : "FAIL");

    delay(2000);  // Mostra tela de boot por 2s antes de prosseguir
}

// ============================================================================
// LOOP — gerencia FSM do OBD client e atualiza display
// ============================================================================
void loop() {
    static uint32_t last_display_update = 0;
    static ObdClientState last_displayed_state = ObdClientState::DISCONNECTED;

    // Processa FSM do cliente OBD (gerencia pairing, AT, handshake, reconexão)
    obd_client_loop();

    // Atualiza display a cada 1s ou quando estado muda
    ObdClientState current_state = obd_client_get_state();
    uint32_t now = millis();

    bool state_changed = (current_state != last_displayed_state);
    bool time_to_update = (now - last_display_update >= 1000);

    if (state_changed || time_to_update) {
        last_display_update = now;
        last_displayed_state = current_state;

        DisplayPayload p = {};
        switch (current_state) {
            case ObdClientState::PAIRING:
            case ObdClientState::INITIALIZING:
            case ObdClientState::HANDSHAKING:
                strncpy(p.line1, "Aguardando",  sizeof(p.line1) - 1);
                strncpy(p.line2, "ELM327...",   sizeof(p.line2) - 1);
                display_render(ScreenId::WAITING, p);
                break;

            case ObdClientState::CONNECTED:
                strncpy(p.line1, "Conectado",   sizeof(p.line1) - 1);
                strncpy(p.line2, "ELM327 OK",   sizeof(p.line2) - 1);
                display_render(ScreenId::BOOT, p);  // Reusa BOOT temporariamente
                break;

            case ObdClientState::DISCONNECTED:
            case ObdClientState::RECOVERING:
            default:
                strncpy(p.line1, "Sem conexao",    sizeof(p.line1) - 1);
                strncpy(p.line2, "Verifique ELM327", sizeof(p.line2) - 1);
                {
                    uint8_t attempts = obd_client_get_reconnect_attempts();
                    if (attempts > 0) {
                        snprintf(p.line3, sizeof(p.line3),
                                 "Tentativas: %u", attempts);
                    } else {
                        strncpy(p.line3, "Iniciando...", sizeof(p.line3) - 1);
                    }
                }
                display_render(ScreenId::NO_CONN, p);
                break;
        }
    }

    // Heartbeat LED: padrão reflete estado
    // CONNECTED: pisca lento (1 Hz)
    // Em transição: pisca rápido (5 Hz)
    // DISCONNECTED/RECOVERING: contínuo
    uint32_t blink_period;
    if (current_state == ObdClientState::CONNECTED) {
        blink_period = 1000;  // 1 Hz
    } else if (current_state == ObdClientState::RECOVERING ||
               current_state == ObdClientState::DISCONNECTED) {
        blink_period = 0;     // Contínuo (solid)
    } else {
        blink_period = 200;   // 5 Hz (em transição)
    }

    if (blink_period == 0) {
        digitalWrite(PIN_LED_STATUS, HIGH);  // Solid
    } else {
        digitalWrite(PIN_LED_STATUS, (now / (blink_period / 2)) % 2);
    }
}