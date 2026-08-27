/**
 * @file rule_engine.h
 * @brief API pública do motor de regras de diagnóstico.
 *
 * Avalia snapshots OBD-II (de obd_client) e produz diagnóstico baseado
 * em regras com histerese e FSM. Três subsistemas monitorados:
 *   - Termostato (P0128)
 *   - Combustão (P0171/P0172, com subtipo vazamento/injetor)
 *   - Catalisador (P0420/P0430)
 *
 * @author Rafael Lucas Junqueira Brigagão (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.1.0
 */

#pragma once

#include <cstdint>
#include <stdbool.h>

#include "obd_pids.h"

// ============================================================================
// Severidade (comum a todas as FSMs)
// ============================================================================
enum class Severity : uint8_t {
    OK        = 0,
    SUSPECT   = 1,
    WARNING   = 2,
    CRITICAL  = 3,
};

// ============================================================================
// Identificadores de subsistema
// ============================================================================
enum class SubsystemId : uint8_t {
    THERMOSTAT   = 0,
    COMBUSTION   = 1,
    CATALYTIC    = 2,
};

// ============================================================================
// Subtipo de falha de combustão
// ============================================================================
enum class CombustionFaultType : uint8_t {
    NONE                = 0,
    LEAN_VACUUM_LEAK    = 1,  // P0171 — desvio só em idle
    LEAN_INJECTOR       = 2,  // P0171 — desvio em toda faixa
    LEAN_INDETERMINATE  = 3,  // P0171 — sem amostra high-RPM ainda
    RICH_VACUUM_LEAK    = 4,  // P0172 — análogo
    RICH_INJECTOR       = 5,
    RICH_INDETERMINATE  = 6,
};

// ============================================================================
// Resultado de diagnóstico por subsistema
// ============================================================================
struct RuleResult {
    Severity    severity;
    SubsystemId subsystem;
    CombustionFaultType combustion_subtype;  // só relevante para COMBUSTION
    char        dtc_code[8];                  // "P0128", "P0171", etc., ou "" se OK
    char        message[32];                  // descrição curta para display
};

// ============================================================================
// Snapshot de diagnóstico (consumido pelo display e logger)
// ============================================================================
struct RuleEngineSnapshot {
    RuleResult thermostat;
    RuleResult combustion;
    RuleResult catalytic;
    Severity   overall;          // max dos três subsistemas
    uint32_t   timestamp_ms;
};

// ============================================================================
// API pública
// ============================================================================

/**
 * @brief Inicializa o motor de regras.
 *
 * Reseta estado interno das FSMs e históricos.
 * @return true se inicializado com sucesso.
 */
bool rule_engine_init();

/**
 * @brief Avalia um snapshot OBD e atualiza estado das FSMs.
 *
 * Deve ser chamada a cada novo snapshot (1 Hz). Aplica histerese internamente.
 *
 * @param obd Snapshot OBD recebido de obd_client_read_all_supported_pids().
 * @param out Snapshot de diagnóstico preenchido pela função.
 * @return true se a avaliação foi executada.
 */
bool rule_engine_evaluate(const ObdSnapshot& obd, RuleEngineSnapshot& out);

/**
 * @brief Retorna a severidade global atual.
 *
 * Útil para o LED pattern selector e para decisões de UI sem precisar do snapshot completo.
 *
 * @return Severidade máxima entre todos os subsistemas.
 */
Severity rule_engine_get_overall_severity();

/**
 * @brief Retorna o número total de transições OK→WARNING registradas.
 *
 * Usado para calcular FPR em M6.
 */
uint32_t rule_engine_get_total_warnings();

/**
 * @brief Retorna o número de "flappings" (OK→WARN→OK em < 5s).
 *
 * Flapping = falso positivo. Métrica de FPR.
 */
uint32_t rule_engine_get_flapping_count();