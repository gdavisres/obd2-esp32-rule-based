/**
 * @file obd_pids.h
 * @brief Tabela de PIDs OBD-II (SAE J1979) e estrutura de snapshot.
 *
 * Define:
 *   - Enum ObdPid com os 10 PIDs do projeto (0x00 + 9 de diagnóstico).
 *   - Struct ObdPidResponse (bytes brutos da resposta).
 *   - Struct ObdSnapshot (valores físicos convertidos, consumidos pela FSM em M4).
 *   - Funções utilitárias para conversão (declaradas aqui, implementadas
 *     em lib/obd_client/obd_client.cpp).
 *
 * Referência: SAE J1979 / ISO 15031-5.
 *
 * @author Gabriel Davi Silva Resende (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.1.0
 */

#pragma once

#include <cstdint>
#include <stdbool.h>

// ============================================================================
// Identificadores de PID (SAE J1979)
// ============================================================================
enum class ObdPid : uint8_t {
    SUPPORTED_PIDS_01_20 = 0x00,  // Bitmask dos PIDs 01-20 suportados
    ECT                  = 0x05,  // Engine Coolant Temperature
    STFT                 = 0x06,  // Short-Term Fuel Trim (Bank 1)
    LTFT                 = 0x07,  // Long-Term Fuel Trim (Bank 1)
    RPM                  = 0x0C,  // Engine RPM
    SPEED                = 0x0D,  // Vehicle Speed
    IAT                  = 0x0F,  // Intake Air Temperature
    THROTTLE_POS         = 0x11,  // Throttle Position
    O2_SENSORS_PRESENT   = 0x13,  // Bitmask dos sensores O2 presentes
    O2_SENSOR_2          = 0x15,  // O2 Sensor 2 (downstream): V e trim%
};

// ============================================================================
// Conversão de enum para string (para logs)
// ============================================================================
inline const char* obd_pid_name(ObdPid pid) {
    switch (pid) {
        case ObdPid::SUPPORTED_PIDS_01_20: return "SUPPORTED_PIDS_01_20";
        case ObdPid::ECT:          return "ECT";
        case ObdPid::STFT:         return "STFT";
        case ObdPid::LTFT:         return "LTFT";
        case ObdPid::RPM:          return "RPM";
        case ObdPid::SPEED:        return "SPEED";
        case ObdPid::IAT:          return "IAT";
        case ObdPid::THROTTLE_POS: return "THROTTLE_POS";
        case ObdPid::O2_SENSORS_PRESENT: return "O2_SENSORS_PRESENT";
        case ObdPid::O2_SENSOR_2:  return "O2_SENSOR_2";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// Resposta bruta de PID (bytes hex recebidos do ELM327)
// ============================================================================
struct ObdPidResponse {
    bool     valid;          // true se a resposta foi parseada com sucesso
    uint8_t  num_bytes;      // Número de bytes de dados (sem contar 0x41 e PID echo)
    uint8_t  data[8];        // Bytes de dados (máx 8 para PIDs de 64-bit)
    uint32_t timestamp_ms;   // Quando a resposta foi recebida (millis())
    uint32_t latency_ms;     // Latência do request (envio → resposta)
};

// ============================================================================
// Snapshot de todos os PIDs em um ciclo de leitura
// ============================================================================
// Consumido pela FSM de regras em M4. Cada campo tem flag _valid porque
// PIDs não suportados pelo veículo não terão valor.
struct ObdSnapshot {
    uint32_t timestamp_ms;  // Quando o snapshot foi gerado
    
    // PIDs rápidos (1 Hz) — dinâmica do motor
    bool     rpm_valid;
    uint16_t rpm;
    bool     speed_valid;
    uint8_t  speed_kmh;
    bool     throttle_valid;
    uint8_t  throttle_percent;
    
    // PIDs médios (1 Hz) — combustão
    bool     stft_valid;
    int8_t   stft_percent;     // Pode ser negativo
    bool     ltft_valid;
    int8_t   ltft_percent;
    
    // PIDs lentos (0.2 Hz) — temperatura
    bool     ect_valid;
    int8_t   ect_celsius;      // Pode ser negativo (até -40)
    bool     iat_valid;
    int8_t   iat_celsius;
    
    // PIDs eventuais (0.5 Hz) — catalisador
    bool     o2_v_valid;
    float    o2_voltage;       // 0-1.275 V
    bool     o2_trim_valid;
    int8_t   o2_trim_percent;
    
    // Métricas de qualidade do ciclo
    uint8_t  pids_attempted;   // Quantos PIDs tentamos ler
    uint8_t  pids_succeeded;   // Quantos retornaram valor válido
    uint32_t cycle_latency_ms; // Latência total do ciclo
};

// ============================================================================
// Funções de conversão (implementadas em lib/obd_client/obd_client.cpp)
// ============================================================================
// Cada função converte os bytes brutos da resposta no valor físico.
// Retornam true se a conversão foi bem-sucedida.

bool convert_ect(const ObdPidResponse& resp, int8_t& out_celsius);
bool convert_stft(const ObdPidResponse& resp, int8_t& out_percent);
bool convert_ltft(const ObdPidResponse& resp, int8_t& out_percent);
bool convert_rpm(const ObdPidResponse& resp, uint16_t& out_rpm);
bool convert_speed(const ObdPidResponse& resp, uint8_t& out_kmh);
bool convert_iat(const ObdPidResponse& resp, int8_t& out_celsius);
bool convert_throttle(const ObdPidResponse& resp, uint8_t& out_percent);
bool convert_o2_voltage(const ObdPidResponse& resp, float& out_volts);
bool convert_o2_trim(const ObdPidResponse& resp, int8_t& out_percent);

// ============================================================================
// Verificação de suporte (bitmask do PID 0x00)
// ============================================================================
// Retorna true se o PID dado é suportado pela ECU, conforme a bitmask
// recebida no handshake.
bool is_pid_supported(ObdPid pid, const uint8_t bitmask[4]);