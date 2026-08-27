/**
 * @file thresholds.h
 * @brief Centralização de thresholds de diagnóstico com referência normativa.
 *
 * Todos os limiares do motor de regras estão aqui. Mudar um threshold
 * não exige refatorar lib/rule_engine/rule_engine.cpp.
 *
 * Referências:
 *   - CARB §1968.2 (Malfunction and Diagnostic System Requirements)
 *   - SAE J2012 (DTC definitions)
 *   - SAE J1979 (PID definitions and ranges)
 *
 * @author Rafael Lucas Junqueira Brigagão (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.1.0
 */

#pragma once

#include <cstdint>

// ============================================================================
// Histerese (comum a todas as FSMs)
// ============================================================================
// Número de ciclos consecutivos (1 Hz) necessários para confirmar transição.
static const uint8_t  HYSTERESIS_CONFIRMATIONS  = 3;
static const uint32_t HYSTERESIS_TIMEOUT_MS     = 30000;  // reset se sem leitura por 30s

// ============================================================================
// Termostato (P0128) — CARB §1968.2
// ============================================================================
static const int16_t ECT_NORMAL_MIN_C       = 90;    // OK: 90-104°C
static const int16_t ECT_NORMAL_MAX_C       = 104;
static const int16_t ECT_SUSPECT_MIN_C      = 86;    // SUSPECT: 86-89°C
static const int16_t ECT_SUSPECT_MAX_C      = 89;
static const int16_t ECT_WARNING_MIN_C      = 71;    // WARNING: 71-85°C (falha parcial)
static const int16_t ECT_WARNING_MAX_C      = 85;
static const int16_t ECT_CRITICAL_BELOW_C   = 71;    // CRITICAL: < 71°C após warm-up
static const int16_t ECT_OVERHEAT_ABOVE_C   = 104;   // CRITICAL: > 104°C (superaquecimento)

// Detecção de warm-up
static const uint32_t WARMUP_TIME_SINCE_BOOT_MS = 300000;  // 5 min
static const int16_t  WARMUP_IAT_THRESHOLD_C    = 30;
static const int16_t  WARMUP_ECT_PEAK_C         = 70;  // se ECT já atingiu 70°C, warm-up feito

// ============================================================================
// Combustão (P0171 lean / P0172 rich) — CARB §1968.2
// ============================================================================
static const int8_t  STFT_WARNING_ABS_PERCENT       = 10;   // |STFT| > 10% = aviso
static const int8_t  STFT_LTFT_SUM_CRITICAL_PERCENT = 15;   // |STFT+LTFT| > 15% = falha
static const int8_t  LTFT_CRITICAL_ABS_PERCENT      = 20;   // |LTFT| > 20% = falha crônica

// Diferenciação vazamento de vácuo vs. injetor
static const uint16_t IDLE_RPM_MAX     = 1000;    // RPM < 1000 = idle
static const uint16_t HIGH_RPM_MIN     = 2500;    // RPM > 2500 = high load
static const int8_t   VACUUM_LEAK_DIFF_PERCENT = 10;  // idle_avg - high_avg > 10% = vac leak
static const uint8_t  STFT_HISTORY_SIZE = 8;     // amostras por categoria

// ============================================================================
// Catalisador (P0420/P0430) — SAE J2012
// ============================================================================
static const float    O2_DOWNSTREAM_HEALTHY_MIN_V       = 0.45f;  // steady ~0.5-0.7 V
static const float    O2_DOWNSTREAM_HEALTHY_MAX_V       = 0.75f;
static const float    O2_TRANSITION_DELTA_V             = 0.40f;  // |Vnovo - Vantigo| > 0.4V = transição
static const uint32_t O2_TRANSITION_WINDOW_MS           = 10000;  // 10s
static const uint8_t  O2_TRANSITION_COUNT_THRESHOLD     = 5;      // > 5 transições em 10s = degradado

// ============================================================================
// Métricas de FPR
// ============================================================================
static const uint32_t FPR_FLAPPING_THRESHOLD_MS = 5000;  // OK→WARN→OK em < 5s = flapping