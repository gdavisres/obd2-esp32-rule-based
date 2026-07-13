/**
 * @file main.cpp
 * @brief Ponto de entrada do firmware — Sistema de Diagnóstico OBD-II.
 *
 * Milestone M0: smoke test do toolchain. Apenas valida que:
 *   1. O PlatformIO compila e flasheia para o ESP32.
 *   2. O Serial está funcional a 115200 bps.
 *   3. O LED onboard (GPIO2) pisca a 1 Hz.
 *
 * Não há lógica de negócio aqui — isso entra a partir de M1.
 *
 * @author Gabriel Davi Silva Resende
 * @license MIT
 * @version 0.1.0
 */

#include <Arduino.h>

// Definições de pinos — centralizadas aqui em M0, migradas para
// include/pinout.h em M1.
static const uint8_t LED_ONBOARD_PIN = 2;   // LED azul onboard do DevKit
static const uint32_t BLINK_PERIOD_MS = 1000; // 1 Hz = 1s período = 500ms on/off

// Versão do firmware, injetada via build_flags no platformio.ini.
static const char* FIRMWARE_VERSION = PROJECT_VERSION;

/**
 * @brief Inicialização — roda uma vez no boot.
 *
 * Configura Serial para debug (115200 bps, compatível com CH340C) e
 * o GPIO do LED onboard como saída. O log de boot segue o formato
 * "chave=valor" para facilitar parsing futuro pelo logger estruturado.
 */
void setup() {
    Serial.begin(115200);
    delay(100);  // Tempo para o CH340C estabilizar a conexão USB.

    Serial.println();
    Serial.println("======================================");
    Serial.println("OBD Diagnostic v" + String(FIRMWARE_VERSION));
    Serial.println("UNIFEI / PET-TEC / UNIGOU 2026");
    Serial.println("Fase: M0 — smoke test do toolchain");
    Serial.println("======================================");
    Serial.println("[boot] status=ok serial=115200 led=GPIO2");
    Serial.println();

    pinMode(LED_ONBOARD_PIN, OUTPUT);
    digitalWrite(LED_ONBOARD_PIN, LOW);
}

/**
 * @brief Loop principal — roda indefinidamente após setup().
 *
 * Em M0, é um blink simples. A partir de M3, este loop NÃO terá
 * lógica de aplicação — apenas criará as tasks FreeRTOS e ficará
 * ocioso (idle task). Toda lógica viverá em tasks.
 */
void loop() {
    digitalWrite(LED_ONBOARD_PIN, HIGH);
    delay(BLINK_PERIOD_MS / 2);

    digitalWrite(LED_ONBOARD_PIN, LOW);
    delay(BLINK_PERIOD_MS / 2);

    // Heartbeat no Serial — prova que o firmware está vivo.
    static uint32_t counter = 0;
    Serial.printf("[heartbeat] tick=%lu\n", counter++);
}