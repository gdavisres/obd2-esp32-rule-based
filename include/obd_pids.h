/**
 * @file obd_pids.h
 * @brief Tabela de PIDs OBD-II (SAE J1979) suportados pelo projeto.
 *
 * Cada PID tem: ID, nome, unidade, fórmula de conversão, e descrição.
 * A fórmula é aplicada aos bytes brutos da resposta ELM327 para obter
 * o valor físico.
 *
 * Referência: SAE J1979 / ISO 15031-5.
 *
 * @author Gabriel Davi Silva Resende (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.1.0
 */

#pragma once

#include <cstdint>

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
// Estrutura de resposta de PID
// ============================================================================
struct ObdPidResponse {
    bool     valid;          // true se a resposta foi parseada com sucesso
    uint8_t  num_bytes;      // Número de bytes de dados (sem contar 0x41 e PID echo)
    uint8_t  data[8];        // Bytes de dados (máx 8 para PIDs de 64-bit)
    uint32_t timestamp_ms;   // Quando a resposta foi recebida (millis())
};

// ============================================================================
// Fórmulas de conversão (implementadas em lib/obd_client/obd_client.cpp)
// ============================================================================
// Exemplos:
//   ECT  = data[0] - 40                  [°C]
//   STFT = (data[0] - 128) * 100 / 128   [%]
//   RPM  = (data[0]*256 + data[1]) / 4   [rpm]
// Velocidade  = data[0]                  [km/h]
//   IAT  = data[0] - 40                  [°C]
//   TP   = data[0] * 100 / 255           [%]
// (implementação completa em M3)