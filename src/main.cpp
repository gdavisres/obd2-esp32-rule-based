/**
 * @file main.cpp
 * @brief Ponto de entrada do firmware — Sistema de Diagnóstico OBD-II.
 *
 * Milestone M0: smoke test do toolchain + inicialização do OLED + blink LED.
 * Estabelece a base que será refatorada em tasks FreeRTOS a partir de M3.
 *
 * Hardware:
 *   - ESP32 DevKit v1 30 pinos (CH340C)
 *   - OLED SSD1306 128x64 I2C (endereço 0x3C)
 *     SDA -> GPIO21, SCL -> GPIO22, VCC -> 3V3, GND -> GND
 *   - LED onboard -> GPIO2
 *
 * @author Gabriel Davi Silva Resende (PET-TEC UNIFEI)
 * @author Vitor Fernandes (colaborador M0)
 * @license MIT
 * @version 0.1.0
 */

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ------- Configurações do Display OLED -------
static const uint8_t  SCREEN_WIDTH   = 128;
static const uint8_t  SCREEN_HEIGHT  = 64;
static const int8_t   OLED_RESET     = -1;          // -1 = compartilha reset do ESP32
static const uint8_t  OLED_ADDRESS   = 0x3C;        // Pode ser 0x3D em alguns módulos

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ------- Configuração do LED Onboard -------
static const uint8_t  LED_PIN        = 2;
static const uint16_t BLINK_INTERVAL = 500;          // ms (500 on + 500 off = 1 Hz)

// ------- Estado do display (fail-soft) -------
static bool display_ok = false;

// ------- Versão do firmware (injetada via build_flags) -------
static const char* FIRMWARE_VERSION = PROJECT_VERSION;


// =============================================================
//  SETUP - Inicialização
// =============================================================
void setup() {
    Serial.begin(115200);
    delay(100);  // Tempo para o CH340C estabilizar a conexão USB.

    // ------- Boot Banner na Serial -------
    Serial.println();
    Serial.println("========================================");
    Serial.println("OBD Diagnostic v" + String(FIRMWARE_VERSION));
    Serial.println("UNIFEI / PET-TEC / UNIGOU 2026");
    Serial.println("Fase: M0 — smoke test + OLED boot + LED blink");
    Serial.println("========================================");
    Serial.println("[boot] status=ok serial=115200 led=GPIO2 oled=0x3C");
    Serial.println();

    // ------- Configuração do LED -------
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // ------- Inicialização do Display OLED (fail-soft) -------
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println(F("[ERRO] Display OLED nao encontrado — operando sem UI"));
        display_ok = false;
    } else {
        display_ok = true;
        Serial.println(F("[OK] Display OLED inicializado"));

        // ------- Tela de Boot no OLED -------
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);

        // Título centralizado
        display.setTextSize(2);
        const char* title = "OBD Diagnostic";
        int16_t  x1, y1;
        uint16_t w, h;
        display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
        display.setCursor((SCREEN_WIDTH - w) / 2, 15);
        display.print(title);

        // Versão centralizada
        display.setTextSize(1);
        display.setCursor((SCREEN_WIDTH - 30) / 2, 40);
        display.print("v");
        display.print(FIRMWARE_VERSION);

        // Status
        display.setCursor(30, 55);
        display.print("Iniciando...");

        display.display();
        Serial.println(F("[OK] Tela de boot exibida"));
    }

    Serial.println();
}


// =============================================================
//  LOOP - Blink do LED a 1 Hz + heartbeat no Serial
// =============================================================
void loop() {
    // LED blink a 1 Hz: 500ms ON, 500ms OFF
    digitalWrite(LED_PIN, HIGH);
    delay(BLINK_INTERVAL);
    digitalWrite(LED_PIN, LOW);
    delay(BLINK_INTERVAL);

    // Heartbeat no Serial — prova que o firmware está vivo.
    // Em M3, este loop() ficará vazio (toda lógica em tasks FreeRTOS).
    static uint32_t counter = 0;
    Serial.printf("[heartbeat] tick=%u\n", static_cast<unsigned int>(counter++));
}