/**
 * @file led_indicator.cpp
 * @brief Implementação do indicador LED com Task_LED orientada a eventos.
 *
 * Arquitetura:
 *   - led_indicator_set_pattern() envia comando via queue.
 *   - Task_LED (interna) recebe comandos e executa padrões com vTaskDelay.
 *   - Non-blocking: usa vTaskDelay, nunca delay().
 *
 * @author Rafael (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.1.0
 */

#include "led_indicator.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "../../include/pinout.h"

// ============================================================================
// Configurações internas
// ============================================================================
static const uint32_t LED_TASK_STACK_SIZE   = 2048;
static const uint8_t  LED_TASK_PRIORITY     = 1;
static const uint8_t  LED_QUEUE_LENGTH      = 1;

// Timing dos padrões (em ms)
static const uint32_t PATTERN_OK_BLINK_MS        = 100;
static const uint32_t PATTERN_OK_PERIOD_MS        = 5000;
static const uint32_t PATTERN_INFO_BLINK_MS       = 100;
static const uint32_t PATTERN_INFO_GAP_MS         = 1000;
static const uint32_t PATTERN_WARN_BLINK_MS        = 100;
static const uint32_t PATTERN_WARN_GAP_MS         = 100;
static const uint32_t PATTERN_WARN_PAUSE_MS       = 1500;
static const uint32_t PATTERN_CRIT_BLINK_MS       = 200;

// ============================================================================
// Estado interno
// ============================================================================
static QueueHandle_t led_queue = nullptr;
static TaskHandle_t  led_task  = nullptr;

static bool apply_pattern_update(LedPattern& current_pattern, TickType_t wait_ticks) {
    TickType_t start = xTaskGetTickCount();
    TickType_t remaining = wait_ticks;

    while (true) {
        LedPattern next_pattern;
        if (xQueueReceive(led_queue, &next_pattern, remaining) != pdTRUE) return false;

        while (xQueueReceive(led_queue, &next_pattern, 0) == pdTRUE) {
            // Mantém apenas o comando mais recente.
        }

        if (next_pattern != current_pattern) {
            current_pattern = next_pattern;
            Serial.printf("[led] padrão: %d\n", static_cast<int>(current_pattern));
            digitalWrite(PIN_LED_STATUS, LOW);  // reset ao mudar
            return true;
        }

        if (wait_ticks == 0) return false;

        TickType_t elapsed = xTaskGetTickCount() - start;
        if (elapsed >= wait_ticks) return false;
        remaining = wait_ticks - elapsed;
    }
}

// ============================================================================
// Task_LED — executa padrões de piscar
// ============================================================================
// Recebe comandos da queue. Para cada padrão, executa sequência de
// vTaskDelay + digitalWrite. Não bloqueia outras tasks.

static void task_led(void* arg) {
    (void)arg;
    LedPattern current_pattern = LedPattern::OFF;
    digitalWrite(PIN_LED_STATUS, LOW);

    while (true) {
        // Verifica se há novo padrão sem bloquear.
        apply_pattern_update(current_pattern, 0);

        // Executa padrão atual
        switch (current_pattern) {
            case LedPattern::OFF:
                digitalWrite(PIN_LED_STATUS, LOW);
                apply_pattern_update(current_pattern, pdMS_TO_TICKS(1000));
                break;

            case LedPattern::OK:
                // 1 piscada curta a cada 5s
                digitalWrite(PIN_LED_STATUS, HIGH);
                if (apply_pattern_update(current_pattern, pdMS_TO_TICKS(PATTERN_OK_BLINK_MS))) break;
                digitalWrite(PIN_LED_STATUS, LOW);
                apply_pattern_update(current_pattern, pdMS_TO_TICKS(PATTERN_OK_PERIOD_MS -
                                          PATTERN_OK_BLINK_MS));
                break;

            case LedPattern::INFO:
                // 1 piscada curta, pausa 1s, repete
                digitalWrite(PIN_LED_STATUS, HIGH);
                if (apply_pattern_update(current_pattern, pdMS_TO_TICKS(PATTERN_INFO_BLINK_MS))) break;
                digitalWrite(PIN_LED_STATUS, LOW);
                apply_pattern_update(current_pattern, pdMS_TO_TICKS(PATTERN_INFO_GAP_MS));
                break;

            case LedPattern::WARNING:
                // 2 piscadas curtas, pausa 1.5s, repete
                digitalWrite(PIN_LED_STATUS, HIGH);
                if (apply_pattern_update(current_pattern, pdMS_TO_TICKS(PATTERN_WARN_BLINK_MS))) break;
                digitalWrite(PIN_LED_STATUS, LOW);
                if (apply_pattern_update(current_pattern, pdMS_TO_TICKS(PATTERN_WARN_GAP_MS))) break;
                digitalWrite(PIN_LED_STATUS, HIGH);
                if (apply_pattern_update(current_pattern, pdMS_TO_TICKS(PATTERN_WARN_BLINK_MS))) break;
                digitalWrite(PIN_LED_STATUS, LOW);
                apply_pattern_update(current_pattern, pdMS_TO_TICKS(PATTERN_WARN_PAUSE_MS));
                break;

            case LedPattern::CRITICAL:
                // Pisca contínuo 200ms ON / 200ms OFF
                digitalWrite(PIN_LED_STATUS, HIGH);
                if (apply_pattern_update(current_pattern, pdMS_TO_TICKS(PATTERN_CRIT_BLINK_MS))) break;
                digitalWrite(PIN_LED_STATUS, LOW);
                apply_pattern_update(current_pattern, pdMS_TO_TICKS(PATTERN_CRIT_BLINK_MS));
                break;

            case LedPattern::SOLID:
                digitalWrite(PIN_LED_STATUS, HIGH);
                apply_pattern_update(current_pattern, pdMS_TO_TICKS(1000));
                break;
        }
    }
}

// ============================================================================
// API pública
// ============================================================================

bool led_indicator_init() {
    if (led_queue != nullptr) return true;  // já inicializado

    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    led_queue = xQueueCreate(LED_QUEUE_LENGTH, sizeof(LedPattern));
    if (led_queue == nullptr) {
        Serial.println("[led] ERRO: falha ao criar queue");
        return false;
    }

    BaseType_t ok = xTaskCreate(
        task_led,
        "Task_LED",
        LED_TASK_STACK_SIZE,
        nullptr,
        LED_TASK_PRIORITY,
        &led_task
    );

    if (ok != pdPASS) {
        vQueueDelete(led_queue);
        led_queue = nullptr;
        Serial.println("[led] ERRO: falha ao criar task");
        return false;
    }

    Serial.println("[led] OK: Task_LED criada, queue ativa");
    return true;
}

void led_indicator_set_pattern(LedPattern pattern) {
    if (led_queue == nullptr) return;
    // Mantém apenas o padrão mais recente, sem bloquear.
    xQueueOverwrite(led_queue, &pattern);
}

void led_indicator_set_severity(Severity severity) {
    LedPattern p;
    switch (severity) {
        case Severity::OK:       p = LedPattern::OK;       break;
        case Severity::SUSPECT:  p = LedPattern::INFO;     break;
        case Severity::WARNING:  p = LedPattern::WARNING;  break;
        case Severity::CRITICAL: p = LedPattern::CRITICAL; break;
        default:                 p = LedPattern::OFF;      break;
    }
    led_indicator_set_pattern(p);
}
