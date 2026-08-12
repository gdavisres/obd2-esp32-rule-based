/**
 * @file obd_client.h
 * @brief API pública do cliente OBD-II (Bluetooth SPP + ELM327).
 *
 * Esta biblioteca encapsula toda a comunicação com o adaptador ELM327 V1.5
 * via Bluetooth Classic SPP. A API é thread-safe via mutex FreeRTOS e pode
 * ser chamada de múltiplas tasks.
 *
 * Em M3, adicionamos:
 *   - Leitura de PIDs com conversão para valores físicos (SAE J1979).
 *   - Cache de PIDs suportados (bitmask do handshake).
 *   - Snapshot agregado para consumo pela FSM em M4.
 *   - Sampling diferenciado por categoria (rápida/média/lenta/eventual).
 *
 * @author Gabriel Davi Silva Resende (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.2.0
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
    PAIRING        = 1,
    INITIALIZING   = 2,
    HANDSHAKING    = 3,
    CONNECTED      = 4,
    RECOVERING     = 5,
};

// ============================================================================
// API pública — conexão (M2)
// ============================================================================

bool obd_client_init();
void obd_client_loop();
bool obd_client_is_connected();
ObdClientState obd_client_get_state();
uint8_t obd_client_get_reconnect_attempts();

// ============================================================================
// API pública — leitura de PIDs (M3)
// ============================================================================

/**
 * @brief Lê um PID específico e retorna o valor físico convertido.
 *
 * Bloqueia até receber a resposta ou timeout (4 s default). Thread-safe.
 *
 * @param pid PID a ser lido.
 * @param response Estrutura onde a resposta bruta será armazenada (inclui
 *                 latency_ms para métricas).
 * @return true se a resposta foi recebida e parseada com sucesso.
 */
bool obd_client_send(ObdPid pid, ObdPidResponse& response);

/**
 * @brief Lê todos os PIDs suportados em um ciclo e preenche o snapshot.
 *
 * Aplica sampling diferenciado:
 *   - PIDs rápidos (RPM, Speed, TP): sempre lidos.
 *   - PIDs médios (STFT, LTFT): sempre lidos.
 *   - PIDs lentos (ECT, IAT): lidos a cada 5 ciclos (5 s @ 1 Hz).
 *   - PIDs eventuais (O2): lidos a cada 2 ciclos (2 s @ 1 Hz).
 *
 * Pula PIDs não suportados pela ECU (verifica cache de bitmask).
 *
 * @param snapshot Estrutura preenchida com os valores lidos.
 * @return true se pelo menos 1 PID foi lido com sucesso.
 */
bool obd_client_read_all_supported_pids(ObdSnapshot& snapshot);

/**
 * @brief Verifica se um PID específico é suportado pela ECU.
 *
 * Usa a bitmask recebida no handshake. Retorna false se o handshake
 * ainda não foi concluído.
 *
 * @param pid PID a verificar.
 * @return true se suportado, false caso contrário.
 */
bool obd_client_is_pid_supported(ObdPid pid);

/**
 * @brief Retorna o número de PIDs suportados pelo veículo (dos 9 monitorados).
 *
 * Útil para diagnóstico e para o display mostrar "X/9 PIDs ativos".
 *
 * @return Contagem de PIDs suportados (0-9).
 */
uint8_t obd_client_count_supported_pids();