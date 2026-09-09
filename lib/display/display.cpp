/**
 * @file display.cpp
 * @brief Implementação do subsistema de display OLED SSD1306.
 *
 * M5 — Refinamentos:
 *   - Refresh inteligente: não redesenha se nada mudou.
 *   - Animações sutis (WARNING: ! pulsante, CRITICAL: borda piscante).
 *   - Layout profissional com hierarquia visual.
 *
 * @author Gabriel Davi Silva Resende (PET-TEC UNIFEI)
 * @author Rafael (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.2.0
 */

#include "display.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <freertos/semphr.h>
#include <cstring>

#include "../../include/pinout.h"

// ============================================================================
// Estado interno
// ============================================================================
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
static SemaphoreHandle_t display_mutex = nullptr;
static bool display_initialized = false;

// Refresh inteligente
static ScreenId last_screen = static_cast<ScreenId>(255);
static uint32_t last_payload_hash = 0;

// ============================================================================
// Hash simples para detectar mudança de payload
// ============================================================================
static uint32_t compute_hash(const DisplayPayload& p) {
    // FNV-1a hash — rápido e suficiente para detectar mudanças
    uint32_t hash = 2166136261UL;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&p);
    for (size_t i = 0; i < sizeof(p); i++) {
        hash ^= data[i];
        hash *= 16777619UL;
    }
    return hash;
}

// ============================================================================
// Funções de desenho auxiliares
// ============================================================================

// Centraliza texto horizontalmente, retorna posição X
static int16_t center_text_x(const char* text, uint8_t text_size = 1) {
    int16_t  x1, y1;
    uint16_t w, h;
    display.setTextSize(text_size);
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return (SCREEN_WIDTH - w) / 2;
}

// Desenha badge retangular com texto (para títulos)
static void draw_badge(const char* text, uint16_t bg_color) {
    int16_t  x1, y1;
    uint16_t w, h;
    display.setTextSize(1);
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int16_t x = (SCREEN_WIDTH - w - 4) / 2;
    display.fillRect(x - 2, 0, w + 4, h + 2, bg_color);
    display.setTextColor(bg_color == SSD1306_WHITE ? SSD1306_BLACK : SSD1306_WHITE);
    display.setCursor(x, 1);
    display.print(text);
    display.setTextColor(SSD1306_WHITE);
}

// ============================================================================
// Funções privadas de renderização (uma por tela)
// ============================================================================

static void render_boot(const DisplayPayload& p) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // Título centralizado, fonte 2
    display.setTextSize(2);
    int16_t x = center_text_x(p.line1, 2);
    display.setCursor(x, 4);
    display.print(p.line1);

    // Versão centralizada
    display.setTextSize(1);
    x = center_text_x(p.line2);
    display.setCursor(x, 30);
    display.print(p.line2);

    // Status
    display.setCursor(0, 50);
    display.print(p.line3);

    display.display();
}

static void render_waiting(const DisplayPayload& p) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // Badge "AGUARDANDO" no topo
    display.setTextSize(1);
    draw_badge("AGUARDANDO", SSD1306_WHITE);

    // Subtítulo
    display.setCursor(0, 20);
    display.print(p.line1);  // "ELM327..."

    // Indicador de atividade: 3 retângulos pulsantes (animados)
    uint32_t t = millis() / 300;  // 300 ms por passo
    uint8_t active = t % 3;
    for (int i = 0; i < 3; i++) {
        if (i == active) {
            display.fillRect(20 + i * 12, 38, 8, 8, SSD1306_WHITE);
        } else {
            display.drawRect(20 + i * 12, 38, 8, 8, SSD1306_WHITE);
        }
    }

    // Footer
    display.setCursor(0, 56);
    display.print(p.line2);

    display.display();
}

static void render_no_conn(const DisplayPayload& p) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // Ícone "X" grande (24x24) à esquerda, com borda
    display.drawRect(2, 2, 26, 26, SSD1306_WHITE);
    display.drawLine( 6,  6, 22, 22, SSD1306_WHITE);
    display.drawLine(22,  6,  6, 22, SSD1306_WHITE);

    // Mensagem à direita
    display.setTextSize(2);
    display.setCursor(34, 4);
    display.print(p.line1);  // "Sem conexao"

    display.setTextSize(1);
    display.setCursor(34, 22);
    display.print(p.line2);  // "Verifique ELM327"

    // Detalhe abaixo
    display.setCursor(0, 38);
    display.print(p.line3);  // "Tentativas: N"

    // Footer com timestamp
    display.drawLine(0, 54, SCREEN_WIDTH, 54, SSD1306_WHITE);
    display.setCursor(0, 56);
    display.print(p.line4);  // "t=Xs"

    display.display();
}

static void render_normal(const DisplayPayload& p) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    // Header compacto: status badge
    draw_badge("OK", SSD1306_WHITE);

    // 4 linhas de dados live
    display.setCursor(0, 18);
    display.print(p.line1);  // ECT, RPM

    display.setCursor(0, 32);
    display.print(p.line2);  // STFT, LTFT

    display.setCursor(0, 46);
    display.print(p.line3);  // Speed, Throttle

    // Linha separadora
    display.drawLine(0, 55, SCREEN_WIDTH, 55, SSD1306_WHITE);
    display.setCursor(0, 57);
    display.print(p.line4);  // PIDs:X/Y

    display.display();
}

static void render_warning(const DisplayPayload& p) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // Badge "AVISO" no topo, com "!" pulsante
    display.fillRect(0, 0, SCREEN_WIDTH, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setTextSize(1);
    display.setCursor(2, 2);
    display.print("! AVISO");

    // "!" pulsante à direita (animação 1 Hz)
    uint32_t t = millis() / 500;
    if (t % 2 == 0) {
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(120, 2);
        display.print("!");
    }

    display.setTextColor(SSD1306_WHITE);

    // Subsistema com problema
    display.setTextSize(1);
    display.setCursor(0, 18);
    display.print(p.line1);  // "WARN: Termostato"

    display.setCursor(0, 32);
    display.print(p.line2);  // mensagem específica

    // Dados live
    display.setCursor(0, 46);
    display.print(p.line3);

    // Footer
    display.drawLine(0, 55, SCREEN_WIDTH, 55, SSD1306_WHITE);
    display.setCursor(0, 57);
    display.print(p.line4);

    display.display();
}

static void render_critical(const DisplayPayload& p) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // Borda piscante (animação 2 Hz) — 4 pixels de espessura
    uint32_t t = millis() / 250;
    if (t % 2 == 0) {
        display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
        display.drawRect(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, SSD1306_WHITE);
    }

    // DTC code em fonte 2, centralizado
    display.setTextSize(2);
    int16_t x = center_text_x(p.line1, 2);
    display.setCursor(x, 6);
    display.print(p.line1);  // "P0128"

    // Mensagem descritiva
    display.setTextSize(1);
    x = center_text_x(p.line2);
    display.setCursor(x, 28);
    display.print(p.line2);  // "Termostato aberto"

    // Subtipo (se houver)
    if (p.line3[0] != '\0') {
        display.setCursor(0, 44);
        display.print(p.line3);
    }

    // Timestamp
    display.setCursor(0, 56);
    display.print(p.line4);

    display.display();
}

// ============================================================================
// API pública
// ============================================================================

bool display_init() {
    if (display_initialized) return true;

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println(F("[display] ERRO: SSD1306 nao encontrado"));
        return false;
    }

    display_mutex = xSemaphoreCreateMutex();
    if (display_mutex == nullptr) {
        Serial.println(F("[display] ERRO: falha ao criar mutex"));
        return false;
    }

    display_initialized = true;
    Serial.println(F("[display] OK: SSD1306 inicializado"));
    return true;
}

bool display_render(ScreenId id, const DisplayPayload& payload) {
    if (!display_initialized) return false;

    // Refresh inteligente: skip se nada mudou
    uint32_t hash = compute_hash(payload);
    bool has_animation = (id == ScreenId::WAITING ||
                          id == ScreenId::WARNING ||
                          id == ScreenId::CRITICAL);
    if (id == last_screen && !has_animation && hash == last_payload_hash) {
        return true;
    }

    xSemaphoreTake(display_mutex, portMAX_DELAY);

    switch (id) {
        case ScreenId::BOOT:     render_boot(payload);     break;
        case ScreenId::WAITING:  render_waiting(payload);  break;
        case ScreenId::NO_CONN:  render_no_conn(payload);  break;
        case ScreenId::NORMAL:   render_normal(payload);   break;
        case ScreenId::WARNING:  render_warning(payload);  break;
        case ScreenId::CRITICAL: render_critical(payload); break;
        default:
            Serial.printf("[display] WARN: tela %d invalida\n",
                          static_cast<int>(id));
            break;
    }

    last_screen = id;
    last_payload_hash = hash;

    xSemaphoreGive(display_mutex);
    return true;
}

bool display_is_ok() {
    return display_initialized;
}
