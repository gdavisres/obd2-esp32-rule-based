/**
 * @file main.cpp
 * @brief Ponto de entrada do firmware — Sistema de Diagnóstico OBD-II.
 *
 * Milestone M5: UI refinada com tasks FreeRTOS separadas para display
 * e LED. main.cpp agora só coordena FSM OBD + rule engine.
 *
 * @author Gabriel Davi Silva Resende (PET-TEC UNIFEI)
 * @author Rafael (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.6.0
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "pinout.h"
#include "display.h"
#include "obd_client.h"
#include "rule_engine.h"
#include "led_indicator.h"

static const char* FIRMWARE_VERSION = PROJECT_VERSION;
static const uint32_t SAMPLING_PERIOD_MS = 1000;
static const uint32_t UI_UPDATE_PERIOD_MS = 200;  // 5 Hz UI refresh
static const uint32_t UI_TASK_STACK_SIZE = 4096;
static const uint8_t UI_TASK_PRIORITY = 1;

static SemaphoreHandle_t ui_state_mutex = nullptr;
static ObdSnapshot last_obd = {};
static RuleEngineSnapshot last_rules = {};
static ObdClientState last_obd_state = ObdClientState::DISCONNECTED;

static void task_ui(void* arg) {
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(UI_UPDATE_PERIOD_MS));

        ObdClientState obd_state = ObdClientState::DISCONNECTED;
        ObdSnapshot obd = {};
        RuleEngineSnapshot rules = {};
        uint32_t now = millis();

        if (ui_state_mutex != nullptr &&
            xSemaphoreTake(ui_state_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            obd_state = last_obd_state;
            obd = last_obd;
            rules = last_rules;
            xSemaphoreGive(ui_state_mutex);
        }

        DisplayPayload p = {};

        if (obd_state == ObdClientState::CONNECTED) {
            Severity overall = rules.overall;

            // Atualiza LED pattern conforme severidade
            led_indicator_set_severity(overall);

            // Atualiza display
            if (overall == Severity::CRITICAL) {
                // Encontrar o pior subsistema
                RuleResult* worst = &rules.thermostat;
                if (rules.combustion.severity == Severity::CRITICAL)
                    worst = &rules.combustion;
                if (rules.catalytic.severity == Severity::CRITICAL)
                    worst = &rules.catalytic;

                strncpy(p.line1, worst->dtc_code, sizeof(p.line1) - 1);
                strncpy(p.line2, worst->message, sizeof(p.line2) - 1);
                if (worst->subsystem == SubsystemId::COMBUSTION) {
                    snprintf(p.line3, sizeof(p.line3), "Sub: %d",
                             static_cast<int>(worst->combustion_subtype));
                }
                snprintf(p.line4, sizeof(p.line4), "t=%us", now / 1000);
                display_render(ScreenId::CRITICAL, p);

            } else if (overall == Severity::WARNING) {
                RuleResult* worst = &rules.thermostat;
                if (rules.combustion.severity == Severity::WARNING)
                    worst = &rules.combustion;
                if (rules.catalytic.severity == Severity::WARNING)
                    worst = &rules.catalytic;

                snprintf(p.line1, sizeof(p.line1), "WARN: %s",
                         worst->subsystem == SubsystemId::THERMOSTAT ? "Termo" :
                         worst->subsystem == SubsystemId::COMBUSTION ? "Comb" : "Cat");
                strncpy(p.line2, worst->message, sizeof(p.line2) - 1);
                snprintf(p.line3, sizeof(p.line3), "ECT=%d RPM=%u",
                         obd.ect_valid ? obd.ect_celsius : 0,
                         obd.rpm_valid ? obd.rpm : 0);
                snprintf(p.line4, sizeof(p.line4), "STFT=%d%% LTFT=%d%%",
                         obd.stft_valid ? obd.stft_percent : 0,
                         obd.ltft_valid ? obd.ltft_percent : 0);
                display_render(ScreenId::WARNING, p);

            } else if (overall == Severity::SUSPECT) {
                // SUSPECT mostra tela NORMAL mas com hint
                snprintf(p.line1, sizeof(p.line1), "ECT:%dC RPM:%u",
                         obd.ect_valid ? obd.ect_celsius : 0,
                         obd.rpm_valid ? obd.rpm : 0);
                snprintf(p.line2, sizeof(p.line2), "STFT:%d%% LTFT:%d%%",
                         obd.stft_valid ? obd.stft_percent : 0,
                         obd.ltft_valid ? obd.ltft_percent : 0);
                snprintf(p.line3, sizeof(p.line3), "SPD:%u TP:%u%%",
                         obd.speed_valid ? obd.speed_kmh : 0,
                         obd.throttle_valid ? obd.throttle_percent : 0);
                snprintf(p.line4, sizeof(p.line4), "Suspect");
                display_render(ScreenId::NORMAL, p);

            } else {  // OK
                snprintf(p.line1, sizeof(p.line1), "ECT:%dC RPM:%u",
                         obd.ect_valid ? obd.ect_celsius : 0,
                         obd.rpm_valid ? obd.rpm : 0);
                snprintf(p.line2, sizeof(p.line2), "STFT:%d%% LTFT:%d%%",
                         obd.stft_valid ? obd.stft_percent : 0,
                         obd.ltft_valid ? obd.ltft_percent : 0);
                snprintf(p.line3, sizeof(p.line3), "SPD:%u TP:%u%%",
                         obd.speed_valid ? obd.speed_kmh : 0,
                         obd.throttle_valid ? obd.throttle_percent : 0);
                snprintf(p.line4, sizeof(p.line4), "PIDs:%u/%u",
                         obd.pids_succeeded,
                         obd.pids_attempted);
                display_render(ScreenId::NORMAL, p);
            }

        } else if (obd_state == ObdClientState::PAIRING ||
                   obd_state == ObdClientState::INITIALIZING ||
                   obd_state == ObdClientState::HANDSHAKING) {
            led_indicator_set_pattern(LedPattern::INFO);
            strncpy(p.line1, "Aguardando", sizeof(p.line1) - 1);
            strncpy(p.line2, "ELM327...", sizeof(p.line2) - 1);
            display_render(ScreenId::WAITING, p);

        } else {  // DISCONNECTED ou RECOVERING
            led_indicator_set_pattern(LedPattern::SOLID);
            strncpy(p.line1, "Sem conexao", sizeof(p.line1) - 1);
            strncpy(p.line2, "Verifique ELM327", sizeof(p.line2) - 1);
            snprintf(p.line3, sizeof(p.line3), "Tentativas: %u",
                     obd_client_get_reconnect_attempts());
            snprintf(p.line4, sizeof(p.line4), "t=%us", now / 1000);
            display_render(ScreenId::NO_CONN, p);
        }
    }
}

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
    Serial.println("Fase: M5 — Interface OLED + Padrões de LED");
    Serial.println("========================================");

    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    bool disp_ok = display_init();
    Serial.printf("[boot] display_init=%s\n", disp_ok ? "OK" : "FAIL");

    if (disp_ok) {
        DisplayPayload p = {};
        strncpy(p.line1, "OBD Diagnostic", sizeof(p.line1) - 1);
        strncpy(p.line2, FIRMWARE_VERSION, sizeof(p.line2) - 1);
        strncpy(p.line3, "Boot M5...",     sizeof(p.line3) - 1);
        display_render(ScreenId::BOOT, p);
    }

    bool obd_ok = obd_client_init();
    Serial.printf("[boot] obd_client_init=%s\n", obd_ok ? "OK" : "FAIL");

    bool rules_ok = rule_engine_init();
    Serial.printf("[boot] rule_engine_init=%s\n", rules_ok ? "OK" : "FAIL");

    bool led_ok = led_indicator_init();
    Serial.printf("[boot] led_indicator_init=%s\n", led_ok ? "OK" : "FAIL");

    ui_state_mutex = xSemaphoreCreateMutex();
    if (ui_state_mutex == nullptr) {
        Serial.println("[boot] ERRO: falha ao criar mutex da UI");
    } else {
        BaseType_t ui_task_ok = xTaskCreate(
            task_ui,
            "Task_UI",
            UI_TASK_STACK_SIZE,
            nullptr,
            UI_TASK_PRIORITY,
            nullptr
        );
        if (ui_task_ok != pdPASS) {
            Serial.println("[boot] ERRO: falha ao criar Task_UI");
        }
    }

    delay(2000);
}

// ============================================================================
// LOOP — coordena FSM OBD + rule engine + atualiza UI/LED via APIs
// ============================================================================
void loop() {
    static uint32_t last_sample_ms = 0;

    obd_client_loop();

    ObdClientState obd_state = obd_client_get_state();
    uint32_t now = millis();

    if (ui_state_mutex != nullptr &&
        xSemaphoreTake(ui_state_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        last_obd_state = obd_state;
        xSemaphoreGive(ui_state_mutex);
    }

    // ---- Sampling OBD a 1 Hz ----
    if (obd_state == ObdClientState::CONNECTED) {
        if (now - last_sample_ms >= SAMPLING_PERIOD_MS) {
            last_sample_ms = now;
            ObdSnapshot sampled_obd = {};
            bool ok = obd_client_read_all_supported_pids(sampled_obd);
            if (ok) {
                RuleEngineSnapshot sampled_rules = {};
                rule_engine_evaluate(sampled_obd, sampled_rules);
                if (ui_state_mutex != nullptr &&
                    xSemaphoreTake(ui_state_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                    last_obd = sampled_obd;
                    last_rules = sampled_rules;
                    xSemaphoreGive(ui_state_mutex);
                }
            }
        }
    }
}