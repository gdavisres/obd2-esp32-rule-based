// =============================================================
//  OBD Diagnostic v0.1 - ESP32 + OLED SSD1306
//  Hello World + OLED Boot + LED Blink
//
//  Hardware:
//    - ESP32 DevKit v1
//    - OLED SSD1306 128x64 I2C (0x3C)
//      SDA -> GPIO 21
//      SCL -> GPIO 22
//      VCC -> 3.3V
//      GND -> GND
//    - LED Onboard -> GPIO 2
//
//  Comandos PlatformIO:
//    pio run              # Compilar
//    pio run -t upload    # Compilar e gravar
//    pio device monitor   # Monitor serial
// =============================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ------- Configurações do Display OLED -------
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define OLED_ADDRESS  0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ------- Configuração do LED Onboard -------
#define LED_PIN         2

// ------- Timing para blink 1 Hz (500ms on, 500ms off) -------
#define BLINK_INTERVAL  500


// =============================================================
//  SETUP - Inicialização
// =============================================================
void setup() {
    // Inicializa Serial a 115200 bps
    Serial.begin(115200);
    
    // Pequeno delay para estabilizar a serial
    delay(100);

    // ------- Boot Banner na Serial -------
    Serial.println(F(""));
    Serial.println(F("========================================"));
    Serial.println(F("  OBD Diagnostic v0.1"));
    Serial.println(F("  Platform: ESP32 DevKit v1"));
    Serial.println(F("  Display:  OLED SSD1306 128x64"));
    Serial.println(F("  Baudrate: 115200"));
    Serial.println(F("========================================"));
    Serial.println(F(""));

    // ------- Configuração do LED -------
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // ------- Inicialização do Display OLED -------
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println(F("[ERRO] Display OLED nao encontrado!"));
        // Pisca LED rapidamente indicando erro
        while (true) {
            digitalWrite(LED_PIN, HIGH);
            delay(100);
            digitalWrite(LED_PIN, LOW);
            delay(100);
        }
    }

    Serial.println(F("[OK] Display OLED inicializado"));

    // ------- Tela de Boot no OLED -------
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // Texto principal centralizado
    display.setTextSize(2);
    String titulo = "OBD Diagnostic";
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(titulo, 0, 0, &x1, &y1, &w, &h);
    int xTitulo = (SCREEN_WIDTH - w) / 2;
    display.setCursor(xTitulo, 15);
    display.print(titulo);

    // Versão
    display.setTextSize(1);
    String versao = "v0.1";
    display.getTextBounds(versao, 0, 0, &x1, &y1, &w, &h);
    int xVersao = (SCREEN_WIDTH - w) / 2;
    display.setCursor(xVersao, 40);
    display.print(versao);

    // Status
    display.setCursor(30, 55);
    display.print("Iniciando...");

    display.display();

    Serial.println(F("[OK] Tela de boot exibida"));
    Serial.println(F(""));
}


// =============================================================
//  LOOP - Blink do LED a 1 Hz
// =============================================================
void loop() {
    // LED blink a 1 Hz: 500ms ON, 500ms OFF
    digitalWrite(LED_PIN, HIGH);
    delay(BLINK_INTERVAL);
    digitalWrite(LED_PIN, LOW);
    delay(BLINK_INTERVAL);
}