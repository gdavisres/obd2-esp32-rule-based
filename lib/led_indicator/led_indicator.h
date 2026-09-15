/**
 * @file led_indicator.h
 * @brief API pública do indicador LED por padrões de piscar.
 *
 * Implementa 4 padrões de piscar por severidade (OK, Info, Warning,
 * Critical) com Task_LED orientada a eventos (non-blocking).
 *
 * Uso típico:
 *   1. led_indicator_init() no setup().
 *   2. led_indicator_set_pattern(Severity::WARNING) para mudar padrão.
 *   3. A task interna executa o padrão automaticamente.
 *
 * @author Rafael (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.1.0
 */

#pragma once

#include <cstdint>
#include <stdbool.h>

#include "rule_engine.h"  // para Severity

// ============================================================================
// Padrões de LED
// ============================================================================
enum class LedPattern : uint8_t {
    OFF       = 0,  // LED apagado (sem task ativa)
    OK        = 1,  // 1 piscada curta a cada 5s (heartbeat)
    INFO      = 2,  // 1 piscada curta, pausa 1s, repete
    WARNING   = 3,  // 2 piscadas curtas, pausa 1.5s, repete
    CRITICAL  = 4,  // Pisca contínuo (200ms ON / 200ms OFF)
};

// ============================================================================
// API pública
// ============================================================================

/**
 * @brief Inicializa o indicador LED.
 *
 * Configura o GPIO do LED e cria Task_LED interna.
 *
 * @return true se inicializado com sucesso.
 */
bool led_indicator_init();

/**
 * @brief Define o padrão atual do LED.
 *
 * Thread-safe: envia comando via queue. A task interna processa.
 *
 * @param pattern Padrão a executar.
 */
void led_indicator_set_pattern(LedPattern pattern);

/**
 * @brief Define o padrão baseado em severidade do rule engine.
 *
 * Mapeamento automático:
 *   Severity::OK       → LedPattern::OK
 *   Severity::SUSPECT  → LedPattern::INFO
 *   Severity::WARNING  → LedPattern::WARNING
 *   Severity::CRITICAL → LedPattern::CRITICAL
 *
 * @param severity Severidade vinda de rule_engine_get_overall_severity().
 */
void led_indicator_set_severity(Severity severity);
