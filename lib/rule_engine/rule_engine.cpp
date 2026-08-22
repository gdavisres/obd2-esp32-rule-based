/**
 * @file rule_engine.cpp
 * @brief Implementação do motor de regras com FSM e histerese.
 *
 * Estrutura:
 *   - Estado interno das 3 FSMs (uma struct por FSM).
 *   - Função privada de histerese genérica (usada pelas 3 FSMs).
 *   - Função privada por FSM: thermostat_fsm, combustion_fsm, catalytic_fsm.
 *   - API pública: rule_engine_init, rule_engine_evaluate, etc.
 *
 * @author Rafael Lucas Junqueira Brigagão (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.1.0
 */

#include "rule_engine.h"

#include <Arduino.h>
#include <cstring>
#include <cstdio>

#include "../../include/thresholds.h"

// ============================================================================
// Estado interno das FSMs
// ============================================================================

struct ThermostatState {
    Severity current_severity;
    uint8_t  confirmation_counter;
    int16_t  ect_peak_since_boot;  // máxima ECT já observada
    bool     warmup_detected;
    uint32_t last_eval_ms;
};

struct CombustionState {
    Severity current_severity;
    uint8_t  confirmation_counter;
    // Histórico de STFT por regime
    int8_t   stft_at_idle_history[STFT_HISTORY_SIZE];
    uint8_t  stft_at_idle_count;
    int8_t   stft_at_high_rpm_history[STFT_HISTORY_SIZE];
    uint8_t  stft_at_high_rpm_count;
    uint8_t  idle_idx;       // próxima posição para escrever (circular)
    uint8_t  high_rpm_idx;
    CombustionFaultType current_subtype;
    uint32_t last_eval_ms;
};

struct CatalyticState {
    Severity current_severity;
    uint8_t  confirmation_counter;
    // Buffer circular de leituras O2 downstream
    float    o2_history[16];
    uint32_t o2_timestamps[16];
    uint8_t  o2_count;
    uint8_t  o2_idx;
    uint32_t last_eval_ms;
};

static ThermostatState  thermostat_state;
static CombustionState  combustion_state;
static CatalyticState   catalytic_state;

// Métricas de FPR
static uint32_t total_warnings   = 0;
static uint32_t flapping_count   = 0;
static uint32_t last_warn_time   = 0;
static Severity last_overall     = Severity::OK;

// ============================================================================
// Funções utilitárias
// ============================================================================

static const char* severity_str(Severity s) {
    switch (s) {
        case Severity::OK:       return "OK";
        case Severity::SUSPECT:  return "SUSPECT";
        case Severity::WARNING:  return "WARN";
        case Severity::CRITICAL: return "CRIT";
        default: return "?";
    }
}

static const char* combustion_subtype_str(CombustionFaultType t) {
    switch (t) {
        case CombustionFaultType::NONE:               return "NONE";
        case CombustionFaultType::LEAN_VACUUM_LEAK:   return "LEAN_VACUUM";
        case CombustionFaultType::LEAN_INJECTOR:      return "LEAN_INJECTOR";
        case CombustionFaultType::LEAN_INDETERMINATE: return "LEAN_INDET";
        case CombustionFaultType::RICH_VACUUM_LEAK:   return "RICH_VACUUM";
        case CombustionFaultType::RICH_INJECTOR:      return "RICH_INJECTOR";
        case CombustionFaultType::RICH_INDETERMINATE: return "RICH_INDET";
        default: return "?";
    }
}

// ============================================================================
// Histerese genérica
// ============================================================================
// Compara severidade avaliada com a atual. Se diferente, incrementa counter.
// Se counter >= HYSTERESIS_CONFIRMATIONS, transita e reseta counter.
// Se igual, reseta counter.

static Severity apply_hysteresis(Severity current, Severity evaluated,
                                  uint8_t& counter) {
    if (evaluated == current) {
        counter = 0;
        return current;
    }

    counter++;
    if (counter >= HYSTERESIS_CONFIRMATIONS) {
        // Transição confirmada
        if (current == Severity::OK && evaluated == Severity::WARNING) {
            total_warnings++;
            last_warn_time = millis();
        }
        // Detecta flapping: se voltou para OK em < 5s após warning
        if (current == Severity::WARNING && evaluated == Severity::OK) {
            if (millis() - last_warn_time < FPR_FLAPPING_THRESHOLD_MS) {
                flapping_count++;
                Serial.println("[rules] WARN: flapping detectado (falso positivo)");
            }
        }
        counter = 0;
        Serial.printf("[rules] transição %s → %s\n",
                      severity_str(current), severity_str(evaluated));
        return evaluated;
    }
    return current;
}

// ============================================================================
// FSM Termostato (P0128)
// ============================================================================

static Severity evaluate_thermostat(const ObdSnapshot& obd,
                                     ThermostatState& state) {
    if (!obd.ect_valid) {
        return Severity::OK;  // sem dado, não avalia
    }

    // Atualiza ECT peak
    if (obd.ect_celsius > state.ect_peak_since_boot) {
        state.ect_peak_since_boot = obd.ect_celsius;
    }

    // Atualiza warm-up
    bool warmup_by_time = (millis() > WARMUP_TIME_SINCE_BOOT_MS);
    bool warmup_by_iat  = (obd.iat_valid && obd.iat_celsius > WARMUP_IAT_THRESHOLD_C);
    bool warmup_by_ect  = (state.ect_peak_since_boot >= WARMUP_ECT_PEAK_C);
    state.warmup_detected = warmup_by_time || warmup_by_iat || warmup_by_ect;

    if (!state.warmup_detected) {
        return Severity::OK;  // motor ainda frio, não avalia
    }

    int16_t ect = obd.ect_celsius;

    if (ect > ECT_OVERHEAT_ABOVE_C) {
        return Severity::CRITICAL;  // superaquecimento (P0217 — fora do escopo)
    }
    if (ect < ECT_CRITICAL_BELOW_C) {
        return Severity::CRITICAL;  // P0128
    }
    if (ect >= ECT_WARNING_MIN_C && ect <= ECT_WARNING_MAX_C) {
        return Severity::WARNING;
    }
    if (ect >= ECT_SUSPECT_MIN_C && ect <= ECT_SUSPECT_MAX_C) {
        return Severity::SUSPECT;
    }
    return Severity::OK;  // 90-104°C
}

static void thermostat_fsm(const ObdSnapshot& obd, RuleResult& out,
                            ThermostatState& state) {
    Severity evaluated = evaluate_thermostat(obd, state);
    state.current_severity = apply_hysteresis(state.current_severity,
                                              evaluated,
                                              state.confirmation_counter);
    state.last_eval_ms = millis();

    out.severity = state.current_severity;
    out.subsystem = SubsystemId::THERMOSTAT;
    out.combustion_subtype = CombustionFaultType::NONE;

    switch (state.current_severity) {
        case Severity::CRITICAL:
            if (obd.ect_valid && obd.ect_celsius < ECT_CRITICAL_BELOW_C) {
                strncpy(out.dtc_code, "P0128", sizeof(out.dtc_code) - 1);
                strncpy(out.message, "Termostato aberto", sizeof(out.message) - 1);
            } else {
                strncpy(out.dtc_code, "P0217", sizeof(out.dtc_code) - 1);
                strncpy(out.message, "Superaquecido", sizeof(out.message) - 1);
            }
            break;
        case Severity::WARNING:
            out.dtc_code[0] = '\0';
            strncpy(out.message, "ECT baixa", sizeof(out.message) - 1);
            break;
        case Severity::SUSPECT:
            out.dtc_code[0] = '\0';
            strncpy(out.message, "ECT subideal", sizeof(out.message) - 1);
            break;
        default:
            out.dtc_code[0] = '\0';
            strncpy(out.message, "OK", sizeof(out.message) - 1);
            break;
    }
}

// ============================================================================
// FSM Combustão (P0171 / P0172)
// ============================================================================

static void push_stft_sample(int8_t value, int8_t* history, uint8_t& count,
                              uint8_t& idx, uint8_t size) {
    history[idx] = value;
    idx = (idx + 1) % size;
    if (count < size) count++;
}

static int8_t avg_stft(const int8_t* history, uint8_t count) {
    if (count == 0) return 0;
    int16_t sum = 0;
    for (uint8_t i = 0; i < count; i++) sum += history[i];
    return static_cast<int8_t>(sum / count);
}

static Severity evaluate_combustion(const ObdSnapshot& obd,
                                     CombustionState& state,
                                     CombustionFaultType& subtype) {
    subtype = CombustionFaultType::NONE;

    if (!obd.stft_valid || !obd.ltft_valid || !obd.rpm_valid) {
        return Severity::OK;
    }

    // Atualiza histórico por regime
    if (obd.rpm < IDLE_RPM_MAX) {
        push_stft_sample(obd.stft_percent, state.stft_at_idle_history,
                         state.stft_at_idle_count, state.idle_idx, STFT_HISTORY_SIZE);
    } else if (obd.rpm > HIGH_RPM_MIN) {
        push_stft_sample(obd.stft_percent, state.stft_at_high_rpm_history,
                         state.stft_at_high_rpm_count, state.high_rpm_idx, STFT_HISTORY_SIZE);
    }

    int8_t stft = obd.stft_percent;
    int8_t ltft = obd.ltft_percent;
    int16_t sum = static_cast<int16_t>(stft) + ltft;

    // CRITICAL: |LTFT| > 20%  OR  |STFT+LTFT| > 15%
    if (ltft > LTFT_CRITICAL_ABS_PERCENT || ltft < -LTFT_CRITICAL_ABS_PERCENT ||
        sum > STFT_LTFT_SUM_CRITICAL_PERCENT ||
        sum < -STFT_LTFT_SUM_CRITICAL_PERCENT) {

        // Diferencia subtipo: lean (positivo) ou rich (negativo)
        bool is_lean = (sum > 0);

        // Compara histórico idle vs high
        int8_t idle_avg = avg_stft(state.stft_at_idle_history,
                                    state.stft_at_idle_count);
        int8_t high_avg = avg_stft(state.stft_at_high_rpm_history,
                                    state.stft_at_high_rpm_count);

        if (state.stft_at_high_rpm_count == 0) {
            // Sem amostras high-RPM ainda
            subtype = is_lean ? CombustionFaultType::LEAN_INDETERMINATE
                              : CombustionFaultType::RICH_INDETERMINATE;
        } else {
            int8_t diff = idle_avg - high_avg;
            if (diff > VACUUM_LEAK_DIFF_PERCENT) {
                // idle muito pior que high → vazamento
                subtype = is_lean ? CombustionFaultType::LEAN_VACUUM_LEAK
                                  : CombustionFaultType::RICH_VACUUM_LEAK;
            } else {
                // desvio similar em toda faixa → injetor
                subtype = is_lean ? CombustionFaultType::LEAN_INJECTOR
                                  : CombustionFaultType::RICH_INJECTOR;
            }
        }
        return Severity::CRITICAL;
    }

    // WARNING: |STFT| > 10%
    if (stft > STFT_WARNING_ABS_PERCENT || stft < -STFT_WARNING_ABS_PERCENT) {
        return Severity::WARNING;
    }

    return Severity::OK;
}

static void combustion_fsm(const ObdSnapshot& obd, RuleResult& out,
                            CombustionState& state) {
    CombustionFaultType subtype;
    Severity evaluated = evaluate_combustion(obd, state, subtype);
    state.current_severity = apply_hysteresis(state.current_severity,
                                              evaluated,
                                              state.confirmation_counter);

    // Subtipo só é válido em CRITICAL
    if (state.current_severity == Severity::CRITICAL) {
        state.current_subtype = subtype;
    } else {
        state.current_subtype = CombustionFaultType::NONE;
    }

    state.last_eval_ms = millis();

    out.severity = state.current_severity;
    out.subsystem = SubsystemId::COMBUSTION;
    out.combustion_subtype = state.current_subtype;

    switch (state.current_severity) {
        case Severity::CRITICAL: {
            bool is_lean = (subtype == CombustionFaultType::LEAN_VACUUM_LEAK ||
                            subtype == CombustionFaultType::LEAN_INJECTOR ||
                            subtype == CombustionFaultType::LEAN_INDETERMINATE);
            strncpy(out.dtc_code, is_lean ? "P0171" : "P0172",
                    sizeof(out.dtc_code) - 1);
            const char* sub = combustion_subtype_str(subtype);
            snprintf(out.message, sizeof(out.message), "%s", sub);
            break;
        }
        case Severity::WARNING:
            out.dtc_code[0] = '\0';
            strncpy(out.message, "STFT alto", sizeof(out.message) - 1);
            break;
        default:
            out.dtc_code[0] = '\0';
            strncpy(out.message, "OK", sizeof(out.message) - 1);
            break;
    }
}

// ============================================================================
// FSM Catalisador (P0420 / P0430)
// ============================================================================

static void push_o2_sample(float voltage, uint32_t ts,
                            CatalyticState& state) {
    state.o2_history[state.o2_idx] = voltage;
    state.o2_timestamps[state.o2_idx] = ts;
    state.o2_idx = (state.o2_idx + 1) % 16;
    if (state.o2_count < 16) state.o2_count++;
}

static uint8_t count_o2_transitions(CatalyticState& state) {
    // Conta transições (|Vn - Vn-1| > DELTA) dentro da janela de 10s
    if (state.o2_count < 2) return 0;

    uint32_t now = millis();
    uint8_t transitions = 0;

    // Percorre o buffer circular em ordem temporal
    // Para simplicidade, assumimos que as amostras estão em ordem
    // (vamos iterar do mais antigo para o mais novo)
    for (uint8_t i = 1; i < state.o2_count; i++) {
        uint8_t idx_curr = (state.o2_idx + 16 - state.o2_count + i) % 16;
        uint8_t idx_prev = (state.o2_idx + 16 - state.o2_count + i - 1) % 16;

        // Filtra por janela temporal
        if (now - state.o2_timestamps[idx_curr] > O2_TRANSITION_WINDOW_MS) {
            continue;
        }

        float delta = state.o2_history[idx_curr] - state.o2_history[idx_prev];
        if (delta < 0) delta = -delta;
        if (delta > O2_TRANSITION_DELTA_V) {
            transitions++;
        }
    }
    return transitions;
}

static Severity evaluate_catalytic(const ObdSnapshot& obd,
                                    CatalyticState& state) {
    if (!obd.o2_v_valid) {
        return Severity::OK;
    }

    push_o2_sample(obd.o2_voltage, millis(), state);

    uint8_t transitions = count_o2_transitions(state);

    Serial.printf("[rules] O2 downstream: V=%.3f, transições(10s)=%u\n",
                  obd.o2_voltage, transitions);

    if (transitions > O2_TRANSITION_COUNT_THRESHOLD) {
        return Severity::CRITICAL;  // P0420
    }
    if (transitions > 2) {
        return Severity::WARNING;
    }
    return Severity::OK;
}

static void catalytic_fsm(const ObdSnapshot& obd, RuleResult& out,
                           CatalyticState& state) {
    Severity evaluated = evaluate_catalytic(obd, state);
    state.current_severity = apply_hysteresis(state.current_severity,
                                              evaluated,
                                              state.confirmation_counter);
    state.last_eval_ms = millis();

    out.severity = state.current_severity;
    out.subsystem = SubsystemId::CATALYTIC;
    out.combustion_subtype = CombustionFaultType::NONE;

    switch (state.current_severity) {
        case Severity::CRITICAL:
            strncpy(out.dtc_code, "P0420", sizeof(out.dtc_code) - 1);
            strncpy(out.message, "Catalisador", sizeof(out.message) - 1);
            break;
        case Severity::WARNING:
            out.dtc_code[0] = '\0';
            strncpy(out.message, "Cat degradando", sizeof(out.message) - 1);
            break;
        default:
            out.dtc_code[0] = '\0';
            strncpy(out.message, "OK", sizeof(out.message) - 1);
            break;
    }
}

// ============================================================================
// API pública
// ============================================================================

bool rule_engine_init() {
    memset(&thermostat_state, 0, sizeof(thermostat_state));
    memset(&combustion_state, 0, sizeof(combustion_state));
    memset(&catalytic_state, 0, sizeof(catalytic_state));

    thermostat_state.ect_peak_since_boot = -100;  // valor sentinela
    combustion_state.current_subtype = CombustionFaultType::NONE;
    last_overall = Severity::OK;

    Serial.println("[rules] motor de regras inicializado");
    return true;
}

bool rule_engine_evaluate(const ObdSnapshot& obd, RuleEngineSnapshot& out) {
    thermostat_fsm(obd, out.thermostat, thermostat_state);
    combustion_fsm(obd, out.combustion, combustion_state);
    catalytic_fsm(obd, out.catalytic, catalytic_state);

    // Severidade global = máximo dos 3 subsistemas
    Severity overall = out.thermostat.severity;
    if (out.combustion.severity > overall) overall = out.combustion.severity;
    if (out.catalytic.severity > overall) overall = out.catalytic.severity;

    out.overall = overall;
    out.timestamp_ms = millis();
    last_overall = overall;

    // Log estruturado
    Serial.printf("DIAG,t=%u,overall=%s,thermo=%s,comb=%s,cat=%s\n",
                  out.timestamp_ms,
                  severity_str(out.overall),
                  severity_str(out.thermostat.severity),
                  severity_str(out.combustion.severity),
                  severity_str(out.catalytic.severity));

    return true;
}

Severity rule_engine_get_overall_severity() {
    return last_overall;
}

uint32_t rule_engine_get_total_warnings() {
    return total_warnings;
}

uint32_t rule_engine_get_flapping_count() {
    return flapping_count;
}