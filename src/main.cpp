/**
 * @file main.cpp
 * @brief Ponto de entrada do firmware — Sistema de Diagnóstico OBD-II.
 *
 * Milestone M3: leitura cíclica de PIDs a 1 Hz com display live.
 * O loop() agora faz polling da FSM OBD e, quando CONNECTED, lê os
 * PIDs suportados a cada 1 segundo e atualiza o display com os valores.
 *
 * @author Gabriel Davi Silva Resende (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.3.0
 */

#include <Arduino.h>

#include "pinout.h"
#include "display.h"
#include "obd_client.h"

static const char* FIRMWARE_VERSION = PROJECT_VERSION;

// Período de sampling: 1 Hz (limitação do ELM327 Bluetooth)
static const uint32_t SAMPLING_PERIOD_MS = 1000;

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
    Serial.println("Fase: M3 — Leitura de PIDs OBD-II");
    Serial.println("========================================");

    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    bool disp_ok = display_init();
    Serial.printf("[boot] display_init=%s\n", disp_ok ? "OK" : "FAIL");

    if (disp_ok) {
        DisplayPayload p = {};
        strncpy(p.line1, "OBD Diagnostic", sizeof(p.line1) - 1);
        strncpy(p.line2, FIRMWARE_VERSION, sizeof(p.line2) - 1);
        strncpy(p.line3, "Boot M3...",     sizeof(p.line3) - 1);
        display_render(ScreenId::BOOT, p);
    }

    bool obd_ok = obd_client_init();
    Serial.printf("[boot] obd_client_init=%s\n", obd_ok ? "OK" : "FAIL");

    delay(2000);
}

// ============================================================================
// LOOP — gerencia FSM OBD e faz polling de PIDs a 1 Hz
// ============================================================================
void loop() {
    static uint32_t last_sample_ms = 0;
    static ObdSnapshot last_snapshot = {};

    obd_client_loop();

    ObdClientState state = obd_client_get_state();
    uint32_t now = millis();

    // Atualiza display conforme estado
    DisplayPayload p = {};

    if (state == ObdClientState::CONNECTED) {
        // Sampling a 1 Hz quando conectado
        if (now - last_sample_ms >= SAMPLING_PERIOD_MS) {
            last_sample_ms = now;
            bool ok = obd_client_read_all_supported_pids(last_snapshot);

            if (ok) {
                // Log estruturado (CSV-like) para análise futura
                Serial.printf("DATA,t=%u,ect=%d,rpm=%u,stft=%d,ltft=%d,speed=%u\n",
                              last_snapshot.timestamp_ms,
                              last_snapshot.ect_valid ? last_snapshot.ect_celsius : -999,
                              last_snapshot.rpm_valid ? last_snapshot.rpm : 0,
                              last_snapshot.stft_valid ? last_snapshot.stft_percent : -999,
                              last_snapshot.ltft_valid ? last_snapshot.ltft_percent : -999,
                              last_snapshot.speed_valid ? last_snapshot.speed_kmh : 0);
            }
        }

        // Renderiza tela NORMAL com valores live
        snprintf(p.line1, sizeof(p.line1), "ECT:%dC RPM:%u",
                 last_snapshot.ect_valid ? last_snapshot.ect_celsius : 0,
                 last_snapshot.rpm_valid ? last_snapshot.rpm : 0);
        snprintf(p.line2, sizeof(p.line2), "STFT:%d%% LTFT:%d%%",
                 last_snapshot.stft_valid ? last_snapshot.stft_percent : 0,
                 last_snapshot.ltft_valid ? last_snapshot.ltft_percent : 0);
        snprintf(p.line3, sizeof(p.line3), "SPD:%u TP:%u%%",
                 last_snapshot.speed_valid ? last_snapshot.speed_kmh : 0,
                 last_snapshot.throttle_valid ? last_snapshot.throttle_percent : 0);
        snprintf(p.line4, sizeof(p.line4), "PIDs:%u/%u OK",
                 last_snapshot.pids_succeeded,
                 last_snapshot.pids_attempted);
        display_render(ScreenId::NORMAL, p);

        // LED: pisca lento quando conectado
        digitalWrite(PIN_LED_STATUS, (now / 500) % 2);

    } else if (state == ObdClientState::PAIRING ||
               state == ObdClientState::INITIALIZING ||
               state == ObdClientState::HANDSHAKING) {
        strncpy(p.line1, "Aguardando", sizeof(p.line1) - 1);
        strncpy(p.line2, "ELM327...", sizeof(p.line2) - 1);
        display_render(ScreenId::WAITING, p);

        digitalWrite(PIN_LED_STATUS, (now / 100) % 2);  // 5 Hz

    } else {  // DISCONNECTED ou RECOVERING
        strncpy(p.line1, "Sem conexao", sizeof(p.line1) - 1);
        strncpy(p.line2, "Verifique ELM327", sizeof(p.line2) - 1);
        snprintf(p.line3, sizeof(p.line3), "Tentativas: %u",
                 obd_client_get_reconnect_attempts());
        display_render(ScreenId::NO_CONN, p);

        digitalWrite(PIN_LED_STATUS, HIGH);  // Solid
    }
}