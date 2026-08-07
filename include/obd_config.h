/**
 * @file obd_config.h
 * @brief Constantes de configuração do subsistema OBD-II.
 *
 * Centraliza parâmetros ajustáveis para facilitar tuning sem tocar no
 * código da biblioteca. Mudar timeouts aqui não exige refatorar
 * lib/obd_client/obd_client.cpp.
 *
 * @author Gabriel Davi Silva Resende (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.1.0
 */

#pragma once

#include <cstdint>

// ============================================================================
// Bluetooth — identificação do ELM327
// ============================================================================
// Nome Bluetooth do ELM327. Varia por fabricante. Valores comuns:
//   "OBDII", "ELM327", "OBDII-BT", "V-LINK"
// Para descobrir o nome exato do seu módulo, escanear dispositivos BT
// a partir do celular ou usar SerialBT.discover() no ESP32.
static const char* ELM327_BT_NAME = "OBDII";

// MAC address do ELM327 (opcional, mais robusto que nome).
// Para descobrir: usar app "Bluetooth Scanner" no celular, ou
// rodar um sketch de BT discovery no ESP32.
// Formato: "XX:XX:XX:XX:XX:XX"
// Exemplo: static const char* ELM327_BT_MAC = "00:1D:A5:00:00:01";
static const char* ELM327_BT_MAC = nullptr;  // nullptr = usar nome, não MAC

// ============================================================================
// Timeouts (ms)
// ============================================================================
static const uint32_t BT_CONNECT_TIMEOUT_MS    = 30000;  // Pairing pode levar até 30s
static const uint32_t AT_COMMAND_TIMEOUT_MS    = 3000;   // Cada AT command: até 3s
static const uint32_t PID_RESPONSE_TIMEOUT_MS  = 4000;   // Cada PID: até 4s
static const uint32_t HANDSHAKE_TIMEOUT_MS     = 5000;   // Handshake 0100: até 5s

// ============================================================================
// Reconexão — backoff exponencial
// ============================================================================
static const uint32_t RECONNECT_BACKOFF_BASE_MS = 15000;   // 15s na primeira tentativa
static const uint32_t RECONNECT_BACKOFF_CAP_MS  = 120000;  // Capping em 2 minutos
static const uint8_t  RECONNECT_MAX_ATTEMPTS    = 5;       // Após 5 tentativas, loga "falha persistente"

// ============================================================================
// Buffer de resposta
// ============================================================================
static const uint16_t OBD_RESPONSE_BUFFER_SIZE = 256;  // Suficiente para qualquer resposta ELM327

// ============================================================================
// PIDs suportados pelo projeto (M3 vai usar todos; M2 usa só 0x00 para handshake)
// ============================================================================
// Para validar o handshake, basta o PID 0x00. Os demais entram em M3.