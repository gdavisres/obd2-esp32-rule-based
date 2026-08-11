/**
 * @file obd_client.h
 * @brief API pública do cliente OBD-II (Bluetooth SPP + ELM327).
 *
 * Esta biblioteca encapsula toda a comunicação com o adaptador ELM327 V1.5
 * via Bluetooth Classic SPP. A API é thread-safe via mutex FreeRTOS e pode
 * ser chamada de múltiplas tasks.
 *
 * Máquina de estados:
 *
 *   DISCONNECTED → PAIRING → INITIALIZING → HANDSHAKING → CONNECTED
 *        ▲                                                       │
 *        │                                                       │ (falha/timeout)
 *        └──────────────── RECOVERING ◄──────────────────────────┘
 *
 * Uso típico:
 *   1. obd_client_init() no setup().
 *   2. obd_client_loop() periodicamente no loop() ou task (gerencia FSM).
 *   3. obd_client_is_connected() para checar estado.
 *   4. obd_client_send(pid) para ler um PID (quando CONNECTED).
 *
 * @author Gabriel Davi Silva Resende (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.1.0
 */

#pragma once

#include <cstdint>
#include <stdbool.h>

#include "obd_pids.h"

// ============================================================================
// Estado da FSM de conexão
// ============================================================================
enum class ObdClientState : uint8_t {
    DISCONNECTED  = 0,
    PAIRING        = 1,   // Tentando conectar via BluetoothSerial
    INITIALIZING   = 2,   // Enviando sequência AT (ATZ → ATSP0)
    HANDSHAKING    = 3,   // Enviando PID 0x00 para validar resposta
    CONNECTED      = 4,   // Pronto para receber requests de PIDs
    RECOVERING     = 5,   // Aguardando backoff antes de tentar reconectar
};

// ============================================================================
// API pública
// ============================================================================

/**
 * @brief Inicializa o cliente OBD-II.
 *
 * Configura BluetoothSerial com PIN default e prepara a FSM. Não conecta
 * imediatamente — a conexão acontece em obd_client_loop().
 *
 * @return true se a inicialização foi bem-sucedida.
 */
bool obd_client_init();

/**
 * @brief Processa a FSM de conexão.
 *
 * Deve ser chamada periodicamente (a cada 100 ms é ideal). Gerencia:
 *   - Tentativas de pairing.
 *   - Sequência de AT commands.
 *   - Handshake 0x00.
 *   - Reconexão com backoff exponencial.
 *
 * Thread-safe: pode ser chamada de qualquer task.
 */
void obd_client_loop();

/**
 * @brief Verifica se o cliente está no estado CONNECTED.
 *
 * @return true se pronto para enviar PIDs.
 */
bool obd_client_is_connected();

/**
 * @brief Retorna o estado atual da FSM.
 *
 * Útil para UI (display mostra WAITING ou NO_CONN baseado no estado).
 *
 * @return Estado atual.
 */
ObdClientState obd_client_get_state();

/**
 * @brief Envia um request de PID ao ELM327.
 *
 * Bloqueia até receber a resposta ou timeout. Thread-safe.
 *
 * @param pid PID a ser lido (ex.: ObdPid::ECT).
 * @param response Ponteiro para estrutura onde a resposta será armazenada.
 * @return true se a resposta foi recebida e parseada com sucesso.
 * @return false se timeout, desconexão, ou resposta inválida.
 */
bool obd_client_send(ObdPid pid, ObdPidResponse& response);

/**
 * @brief Retorna o número de tentativas de reconexão atuais.
 *
 * Útil para debug e UI.
 *
 * @return Número de tentativas (0 se nunca tentou reconectar).
 */
uint8_t obd_client_get_reconnect_attempts();