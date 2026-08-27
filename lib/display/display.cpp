/**
 * @file display.cpp
 * @brief Implementação do subsistema de display OLED SSD1306.
 *
 * Detalhes de implementação:
 *   - Usa Adafruit_SSD1306 + Adafruit_GFX (decisão ADR-0003).
 *   - Mutex FreeRTOS para thread-safety.
 *   - Framebuffer da Adafruit (1 KB em RAM) é interno à biblioteca;
 *     não há alocação dinâmica nesta camada.
 *   - Cada screen_id tem uma função privada render_*() específica.
 *
 * @author Gabriel Davi Silva Resende (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.1.0
 */

#include "display.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <freertos/semphr.h>

#include "../../include/pinout.h"

// ============================================================================
// Estado interno (não exposto no header)
// ============================================================================
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
static SemaphoreHandle_t display_mutex = nullptr;
static bool display_initialized = false;

// O OLED físico é bicolor por faixas; a biblioteca continua sendo monocromática.
// O layout precisa evitar quebra automática de linha para preservar a leitura.

static uint8_t fit_text_size(const char* text, uint8_t preferred_size, uint16_t max_width) {
    if (text == nullptr || text[0] == '\0') {
        return preferred_size;
    }

    int16_t x1, y1;
    uint16_t w, h;
    uint8_t size = preferred_size;

    while (size > 1) {
        display.setTextSize(size);
        display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
        if (w <= max_width) {
            break;
        }
        --size;
    }

    return size;
}

static void draw_centered_text(const char* text, uint8_t preferred_size, int16_t baseline_y) {
    if (text == nullptr || text[0] == '\0') {
        return;
    }

    int16_t x1, y1;
    uint16_t w, h;
    uint8_t size = fit_text_size(text, preferred_size, SCREEN_WIDTH - 8);

    display.setTextSize(size);
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

    int16_t x = (SCREEN_WIDTH - static_cast<int16_t>(w)) / 2;
    if (x < 0) {
        x = 0;
    }

    display.setCursor(x, baseline_y);
    display.print(text);
}

static void draw_left_text(const char* text, uint8_t preferred_size, int16_t left_x, int16_t baseline_y, uint16_t max_width) {
    if (text == nullptr || text[0] == '\0') {
        return;
    }

    int16_t x1, y1;
    uint16_t w, h;
    uint8_t size = fit_text_size(text, preferred_size, max_width);

    display.setTextSize(size);
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(left_x, baseline_y);
    display.print(text);
}

// ============================================================================
// Funções privadas de renderização (uma por tela)
// ============================================================================
// Cada função assume que o mutex JÁ FOI tomado pelo chamador (display_render).
// Não chamar diretamente estas funções de fora do arquivo.

static void render_boot(const DisplayPayload& p) {
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(SSD1306_WHITE);

    // Cabeçalho comum às três telas.
    draw_centered_text(p.line1, 1, 2);

    // Separador entre cabeçalho e conteúdo.
    display.drawFastHLine(0, 16, SCREEN_WIDTH, SSD1306_WHITE);

    // Versão / subtítulo no corpo da tela.
    draw_centered_text(p.line2, 2, 30);

    // Rodapé de status.
    display.setTextSize(1);
    display.setCursor(0, 56);
    display.print(p.line3);

    display.display();
}

static void render_waiting(const DisplayPayload& p) {
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(SSD1306_WHITE);

    // Cabeçalho comum às três telas.
    draw_centered_text(p.line1, 1, 2);

    // Separador entre cabeçalho e conteúdo.
    display.drawFastHLine(0, 16, SCREEN_WIDTH, SSD1306_WHITE);

    // Corpo principal da tela.
    draw_centered_text(p.line2, 2, 28);

    // Indicador de atividade: três retângulos pulsantes.
    // (estático em M1; animação entraria em M5 com task periódica)
    for (int i = 0; i < 3; i++) {
        display.drawRect(86 + i * 12, 50, 8, 8, SSD1306_WHITE);
    }

    display.setTextSize(1);
    display.setCursor(0, 56);
    display.print(p.line3);

    display.display();
}

static void render_no_conn(const DisplayPayload& p) {
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(SSD1306_WHITE);

    // Cabeçalho comum às três telas.
    draw_centered_text(p.line1, 1, 2);

    // Separador entre cabeçalho e conteúdo.
    display.drawFastHLine(0, 16, SCREEN_WIDTH, SSD1306_WHITE);

    // Ícone de erro e mensagem principal no corpo da tela.
    display.drawLine(4, 20, 20, 36, SSD1306_WHITE);
    display.drawLine(20, 20, 4, 36, SSD1306_WHITE);
    display.drawRect(2, 18, 22, 22, SSD1306_WHITE);
    draw_left_text(p.line2, 2, 30, 28, SCREEN_WIDTH - 34);

    // Dica de diagnóstico.
    display.setTextSize(1);
    display.setCursor(0, 56);
    display.print(p.line3);

    display.display();
}

static void render_normal(const DisplayPayload& p) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // 4 linhas de texto, fonte 1 (8 px), espaçamento 14 px
    display.setTextSize(1);

    display.setCursor(0, 0);
    display.print(p.line1);

    display.setCursor(0, 16);
    display.print(p.line2);

    display.setCursor(0, 32);
    display.print(p.line3);

    display.setCursor(0, 48);
    display.print(p.line4);

    // Linha separadora entre dados e status (linha 4)
    display.drawLine(0, 45, SCREEN_WIDTH, 45, SSD1306_WHITE);

    display.display();
}

static void render_warning(const DisplayPayload& p) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // Faixa superior indicando WARNING (texto invertido)
    display.fillRect(0, 0, SCREEN_WIDTH, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setTextSize(1);
    display.setCursor(2, 2);
    display.print("! AVISO !");
    display.setTextColor(SSD1306_WHITE);

    // 3 linhas de detalhe
    display.setTextSize(1);
    display.setCursor(0, 18);
    display.print(p.line1);

    display.setCursor(0, 32);
    display.print(p.line2);

    display.setCursor(0, 46);
    display.print(p.line3);

    // Linha inferior de status
    display.drawLine(0, 56, SCREEN_WIDTH, 56, SSD1306_WHITE);
    display.setCursor(0, 57);
    display.print(p.line4);

    display.display();
}

static void render_critical(const DisplayPayload& p) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // Borda espessa (4px) indicando CRITICAL
    display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    display.drawRect(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, SSD1306_WHITE);

    // DTC code em fonte grande (centralizado)
    display.setTextSize(2);
    int16_t  x1, y1;
    uint16_t w, h;
    display.getTextBounds(p.line1, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 8);
    display.print(p.line1);  // DTC code (ex.: "P0128")

    // Mensagem descritiva (fonte 1)
    display.setTextSize(1);
    display.getTextBounds(p.line2, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 32);
    display.print(p.line2);  // ex.: "Termostato aberto"

    // Subtipo (se houver)
    display.setCursor(0, 48);
    display.print(p.line3);

    // Timestamp
    display.setCursor(0, 56);
    display.print(p.line4);

    display.display();
}

// ============================================================================
// Implementação da API pública
// ============================================================================

bool display_init() {
    if (display_initialized) {
        return true;  // Idempotente: chamar de novo não causa erro
    }

    // Inicializa I2C com os pinos definidos em pinout.h
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);

    // Tenta inicializar o SSD1306
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println(F("[display] ERRO: SSD1306 nao encontrado no barramento I2C"));
        return false;
    }

    // Cria mutex para serializar chamadas a display_render
    display_mutex = xSemaphoreCreateMutex();
    if (display_mutex == nullptr) {
        Serial.println(F("[display] ERRO: falha ao criar mutex"));
        return false;
    }

    display.setTextWrap(false);
    display_initialized = true;
    Serial.println(F("[display] OK: SSD1306 inicializado, mutex criado"));
    return true;
}

bool display_render(ScreenId id, const DisplayPayload& payload) {
    if (!display_initialized) {
        return false;
    }

    // Toma o mutex. portMAX_DELAY = bloqueia indefinidamente até liberar.
    // Em M3+, podemos querer um timeout (ex.: 100 ms) para evitar deadlock
    // se uma task esquecer de liberar. Por ora, bloqueio infinito é seguro.
    if (xSemaphoreTake(display_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    switch (id) {
        case ScreenId::BOOT:
            render_boot(payload);
            break;
        case ScreenId::WAITING:
            render_waiting(payload);
            break;
        case ScreenId::NO_CONN:
            render_no_conn(payload);
            break;
        case ScreenId::NORMAL:
            render_normal(payload);
            break;
        case ScreenId::WARNING:
            render_warning(payload);
            break;
        case ScreenId::CRITICAL:
            render_critical(payload);
            break;
        default:
            Serial.printf("[display] WARN: tela %d invalida\n",
                        static_cast<int>(id));
            break;
    }

    xSemaphoreGive(display_mutex);
    return true;
}

bool display_is_ok() {
    return display_initialized;
}
