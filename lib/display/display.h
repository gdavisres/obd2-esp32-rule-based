/**
 * @file display.h
 * @brief API pública do subsistema de display OLED SSD1306.
 *
 * Esta biblioteca encapsula a comunicação com o display OLED 128x64 via I2C.
 * A API é thread-safe: todas as chamadas a display_render() são serializadas
 * por um mutex FreeRTOS, permitindo que a função seja chamada de múltiplas
 * tasks sem race conditions no framebuffer.
 *
 * Uso típico:
 *   1. Chamar display_init() uma vez no setup().
 *   2. Chamar display_render(screen_id, payload) sempre que quiser mudar a tela.
 *
 * @author Gabriel Davi Silva Resende (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.1.0
 */

#pragma once

#include <cstdint>
#include <stdbool.h>

// Forward declaration para evitar incluir Adafruit_SSD1306.h no header público.
// Reduz acoplamento: quem usa a API não precisa saber dos detalhes da Adafruit.
class Adafruit_SSD1306;

// ============================================================================
// Identificadores de tela (um por "página" da UI)
// ============================================================================
enum class ScreenId : uint8_t {
    BOOT      = 0,  // Tela de boot: nome do projeto + versão
    WAITING   = 1,  // Aguardando conexão com ELM327
    NO_CONN   = 2,  // Falha na conexão com ELM327 (ex.: Bluetooth pareou mas ELM327 não responde)
    NORMAL    = 3,  // Funcionamento normal — parâmetros dentro da faixa (M3+)
    WARNING   = 4,  // Aviso — parâmetro fora de faixa mas não crítico (M5+)
    CRITICAL  = 5,  // Crítico — DTC detectado (M5+)
};

// ============================================================================
// Payload enviado para display_render()
// ============================================================================
// Estrutura fixa (sem alocação dinâmica) com 4 linhas de até 23 caracteres
// + terminador nulo. Suficiente para todas as telas previstas em M1-M5.
//
// Para a tela BOOT, line1 = "OBD Diagnostic", line2 = versão, line3 = status.
// Para a tela WAITING, line1 = "Aguardando", line2 = "ELM327...", line3/4 vazias.
// Para a tela NO_CONN, line1 = "Sem conexao", line2 = "Verifique ELM327".
struct DisplayPayload {
    char line1[24];
    char line2[24];
    char line3[24];
    char line4[24];
};

// ============================================================================
// API pública
// ============================================================================

/**
 * @brief Inicializa o display OLED.
 *
 * Deve ser chamada uma única vez no setup(). Cria o mutex interno e
 * inicializa o controlador SSD1306 com a configuração de pinos definida
 * em pinout.h.
 *
 * @return true se o display foi inicializado com sucesso.
 * @return false se o display não foi encontrado no barramento I2C.
 *         O firmware pode continuar operando sem UI (fail-soft).
 */
bool display_init();

/**
 * @brief Renderiza uma tela específica no display.
 *
 * Thread-safe: serializa acesso via mutex FreeRTOS. Pode ser chamada de
 * qualquer task. Se outra task estiver renderizando, esta chamada bloqueia
 * até o mutex ser liberado (portMAX_DELAY).
 *
 * @param id Identificador da tela a renderizar.
 * @param payload Dados a exibir (linhas de texto). Para telas que não usam
 *                todas as linhas, as não usadas devem ser string vazia ("").
 * @return true se a renderização foi feita com sucesso.
 * @return false se o display não foi inicializado ou id é inválido.
 */
bool display_render(ScreenId id, const DisplayPayload& payload);

/**
 * @brief Verifica se o display foi inicializado com sucesso.
 *
 * Útil para decisões de fail-soft: se display_is_ok() retornar false,
 * o firmware pode pular chamadas a display_render() para economizar tempo.
 *
 * @return true se display_init() foi chamado com sucesso.
 */
bool display_is_ok();