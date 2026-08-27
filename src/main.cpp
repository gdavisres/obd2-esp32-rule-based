/**
 * @file main.cpp
 * @brief Ponto de entrada do firmware — Sistema de Diagnóstico OBD-II.
 *
 * Milestone M4: integração do motor de regras. A cada snapshot OBD,
 * rule_engine_evaluate() é chamado e o display reage à severidade.
 *
 * @author Gabriel Davi Silva Resende (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.5.0
 */

#include <Arduino.h>

#include "pinout.h"
#include "display.h"
#include "obd_client.h"
#include "rule_engine.h"

static const char* FIRMWARE_VERSION = PROJECT_VERSION;

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
    Serial.println("Fase: M4 — Motor de regras + FSM + histerese");
    Serial.println("========================================");

    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    bool disp_ok = display_init();
    Serial.printf("[boot] display_init=%s\n", disp_ok ? "OK" : "FAIL");

    if (disp_ok) {
        DisplayPayload p = {};
        strncpy(p.line1, "OBD Diagnostic", sizeof(p.line1) - 1);
        strncpy(p.line2, FIRMWARE_VERSION, sizeof(p.line2) - 1);
        strncpy(p.line3, "Boot M4...",     sizeof(p.line3) - 1);
        display_render(ScreenId::BOOT, p);
    }

    bool obd_ok = obd_client_init();
    Serial.printf("[boot] obd_client_init=%s\n", obd_ok ? "OK" : "FAIL");

    bool rules_ok = rule_engine_init();
    Serial.printf("[boot] rule_engine_init=%s\n", rules_ok ? "OK" : "FAIL");

    delay(2000);
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
    static uint32_t last_sample_ms = 0;
    static ObdSnapshot last_obd = {};
    static RuleEngineSnapshot last_rules = {};

    obd_client_loop();

    ObdClientState obd_state = obd_client_get_state();
    uint32_t now = millis();

    DisplayPayload p = {};

    if (obd_state == ObdClientState::CONNECTED) {
        // Sampling a 1 Hz
        if (now - last_sample_ms >= SAMPLING_PERIOD_MS) {
            last_sample_ms = now;
            bool ok = obd_client_read_all_supported_pids(last_obd);
            if (ok) {
                rule_engine_evaluate(last_obd, last_rules);
            }
        }

        // Renderiza display conforme severidade global
        Severity overall = last_rules.overall;

        if (overall == Severity::CRITICAL) {
            // Tela CRITICAL: mostra pior subsistema
            RuleResult* worst = &last_rules.thermostat;
            if (last_rules.combustion.severity == Severity::CRITICAL)
                worst = &last_rules.combustion;
            if (last_rules.catalytic.severity == Severity::CRITICAL)
                worst = &last_rules.catalytic;

            strncpy(p.line1, worst->dtc_code, sizeof(p.line1) - 1);
            strncpy(p.line2, worst->message, sizeof(p.line2) - 1);
            // Subtipo para combustão
            if (worst->subsystem == SubsystemId::COMBUSTION) {
                snprintf(p.line3, sizeof(p.line3), "Sub: %d",
                         static_cast<int>(worst->combustion_subtype));
            } else {
                p.line3[0] = '\0';
            }
            snprintf(p.line4, sizeof(p.line4), "t=%us", now / 1000);
            display_render(ScreenId::CRITICAL, p);

            // LED contínuo
            digitalWrite(PIN_LED_STATUS, HIGH);

        } else if (overall == Severity::WARNING) {
            // Tela WARNING
            RuleResult* worst = &last_rules.thermostat;
            if (last_rules.combustion.severity == Severity::WARNING)
                worst = &last_rules.combustion;
            if (last_rules.catalytic.severity == Severity::WARNING)
                worst = &last_rules.catalytic;

            snprintf(p.line1, sizeof(p.line1), "WARN: %s",
                     worst->subsystem == SubsystemId::THERMOSTAT ? "Termo" :
                     worst->subsystem == SubsystemId::COMBUSTION ? "Comb" : "Cat");
            strncpy(p.line2, worst->message, sizeof(p.line2) - 1);
            snprintf(p.line3, sizeof(p.line3), "ECT=%d RPM=%u",
                     last_obd.ect_valid ? last_obd.ect_celsius : 0,
                     last_obd.rpm_valid ? last_obd.rpm : 0);
            snprintf(p.line4, sizeof(p.line4), "STFT=%d%% LTFT=%d%%",
                     last_obd.stft_valid ? last_obd.stft_percent : 0,
                     last_obd.ltft_valid ? last_obd.ltft_percent : 0);
            display_render(ScreenId::WARNING, p);

            // LED: 2 piscadas curtas, pausa, repete
            uint32_t phase = (now % 1500);
            if (phase < 200) digitalWrite(PIN_LED_STATUS, HIGH);
            else if (phase < 400) digitalWrite(PIN_LED_STATUS, LOW);
            else if (phase < 600) digitalWrite(PIN_LED_STATUS, HIGH);
            else digitalWrite(PIN_LED_STATUS, LOW);

        } else {
            // Tela NORMAL (OK ou SUSPECT)
            snprintf(p.line1, sizeof(p.line1), "ECT:%dC RPM:%u",
                     last_obd.ect_valid ? last_obd.ect_celsius : 0,
                     last_obd.rpm_valid ? last_obd.rpm : 0);
            snprintf(p.line2, sizeof(p.line2), "STFT:%d%% LTFT:%d%%",
                     last_obd.stft_valid ? last_obd.stft_percent : 0,
                     last_obd.ltft_valid ? last_obd.ltft_percent : 0);
            snprintf(p.line3, sizeof(p.line3), "SPD:%u TP:%u%%",
                     last_obd.speed_valid ? last_obd.speed_kmh : 0,
                     last_obd.throttle_valid ? last_obd.throttle_percent : 0);
            snprintf(p.line4, sizeof(p.line4), "PIDs:%u/%u Sev:%d",
                     last_obd.pids_succeeded,
                     last_obd.pids_attempted,
                     static_cast<int>(overall));
            display_render(ScreenId::NORMAL, p);

            // LED: pisca lento (1 Hz) quando OK
            digitalWrite(PIN_LED_STATUS, (now / 500) % 2);
        }

    } else if (obd_state == ObdClientState::PAIRING ||
               obd_state == ObdClientState::INITIALIZING ||
               obd_state == ObdClientState::HANDSHAKING) {
        strncpy(p.line1, "Aguardando", sizeof(p.line1) - 1);
        strncpy(p.line2, "ELM327...", sizeof(p.line2) - 1);
        display_render(ScreenId::WAITING, p);
        digitalWrite(PIN_LED_STATUS, (now / 100) % 2);

    } else {
        strncpy(p.line1, "Sem conexao", sizeof(p.line1) - 1);
        strncpy(p.line2, "Verifique ELM327", sizeof(p.line2) - 1);
        snprintf(p.line3, sizeof(p.line3), "Tentativas: %u",
                 obd_client_get_reconnect_attempts());
        display_render(ScreenId::NO_CONN, p);
        digitalWrite(PIN_LED_STATUS, HIGH);
    }
}