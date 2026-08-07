/**
 * @file obd_client.cpp
 * @brief Implementação do cliente OBD-II (Bluetooth SPP + ELM327 V1.5).
 *
 * Detalhes de implementação:
 *   - Usa BluetoothSerial.h do core Arduino-ESP32.
 *   - FSM de conexão com 6 estados.
 *   - Sequência AT: ATZ → ATE0 → ATL0 → ATS0 → ATH0 → ATSP0.
 *   - Handshake: PID 0x00, valida resposta 41 00 <bitmask>.
 *   - Reconexão: backoff exponencial (15s, 30s, 60s, 120s cap).
 *   - Parser robusto: filtra NO DATA, UNABLE TO CONNECT, ?, SEARCHING.
 *   - Mutex FreeRTOS para thread-safety.
 *
 * Referência: ELM327 Datasheet v1.5 (ELM327DS_v1.5.pdf).
 *
 * @author Gabriel Davi Silva Resende (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.1.0
 */

#include "obd_client.h"

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <freertos/semphr.h>
#include <cstring>
#include <cstdio>

#include "../../include/obd_config.h"
#include "../../include/obd_pids.h"

// ============================================================================
// Estado interno
// ============================================================================

// Protótipo antecipado para que fsm_handshaking() possa chamar a versão
// "locked" da função de envio, que assume o mutex já tomado.
static bool obd_client_send_locked(ObdPid pid, ObdPidResponse& response);

static BluetoothSerial serialBT;
static SemaphoreHandle_t obd_mutex = nullptr;

static ObdClientState current_state = ObdClientState::DISCONNECTED;
static uint8_t  reconnect_attempts  = 0;
static uint32_t last_state_change_ms = 0;
static uint32_t last_loop_call_ms    = 0;

// Buffer de recepção
static char response_buffer[OBD_RESPONSE_BUFFER_SIZE];
static uint16_t response_len = 0;

// Índice do comando AT atual (na sequência de init)
static uint8_t at_command_index = 0;


// ============================================================================
// Sequência de comandos AT de inicialização
// ============================================================================
static const char* AT_INIT_SEQUENCE[] = {
    "ATZ",    // Reset
    "ATE0",   // Echo off
    "ATL0",   // Linefeeds off
    "ATS0",   // Spaces off
    "ATH0",   // Headers off
    "ATSP0",  // Protocol auto
};
static const uint8_t AT_INIT_SEQUENCE_LEN = 
    sizeof(AT_INIT_SEQUENCE) / sizeof(AT_INIT_SEQUENCE[0]);

// ============================================================================
// Funções privadas — utilidades
// ============================================================================

static uint32_t calculate_backoff_delay(uint8_t attempt) {
    // Backoff exponencial: 15s * 2^attempt, com cap em 120s
    if (attempt == 0) return RECONNECT_BACKOFF_BASE_MS;
    uint32_t delay_ms = RECONNECT_BACKOFF_BASE_MS * (1UL << attempt);
    if (delay_ms > RECONNECT_BACKOFF_CAP_MS) {
        delay_ms = RECONNECT_BACKOFF_CAP_MS;
    }
    return delay_ms;
}

static void change_state(ObdClientState new_state) {
    if (current_state == new_state) return;
    Serial.printf("[obd] state: %d → %d\n",
                  static_cast<int>(current_state),
                  static_cast<int>(new_state));
    current_state = new_state;
    last_state_change_ms = millis();
}

static void reset_state_machine() {
    response_len = 0;
    at_command_index = 0;
    serialBT.flush();
}

// ============================================================================
// Parser de resposta
// ============================================================================

static bool is_error_response(const char* response) {
    if (response == nullptr) return false;
    // Erros vêm como resposta completa, não como substring.
    // strncmp evita falsos positivos (ex.: resposta que contenha "NO DATA" como parte do valor).
    return strncmp(response, "NO DATA", 7) == 0 ||
           strncmp(response, "UNABLE TO CONNECT", 18) == 0 ||
           strncmp(response, "BUS INIT", 8) == 0 ||
           strncmp(response, "DATA ERROR", 10) == 0 ||
           strcmp(response, "?") == 0;
}
// Marcar como maybe_unused porque será usada em M3 (parser de PIDs em runtime).
// Em M2 ainda não há chamada, mas mantemos a função para não retrabalhar depois.
[[maybe_unused]]
static bool is_searching(const char* response) {
    // O ELM327 envia "SEARCHING..." enquanto tenta localizar a ECU.
    // Não é erro — sinaliza que ainda está processando.
    return strstr(response, "SEARCHING") != nullptr;
}

static bool parse_pid_response(const char* response, ObdPid expected_pid,
                                ObdPidResponse& out) {
    // Resposta esperada (sem espaços, sem headers): 41 <PID> <data...>
    // Exemplo: "4105 5A" para ECT = 90°C (0x5A = 90, 90-40 = 50°C... wait)
    //   (ECT = A - 40, então 0x5A = 90 → 90 - 40 = 50°C)
    
    if (response == nullptr || response[0] == '\0') {
        return false;
    }

    // Verifica prefixo "41" (resposta ao modo 01)
    if (response[0] != '4' || response[1] != '1') {
        return false;
    }

    // Próximos 2 chars: PID em hex
    char pid_hex[3] = {response[2], response[3], '\0'};
    uint8_t pid_value;
    if (sscanf(pid_hex, "%hhx", &pid_value) != 1) {
        return false;
    }
    if (pid_value != static_cast<uint8_t>(expected_pid)) {
        return false;
    }

    // Restante: bytes de dados em hex
    size_t idx = 4;
    uint8_t data[8];
    uint8_t data_count = 0;
    while (response[idx] != '\0' && response[idx] != '\r' &&
           data_count < 8) {
        // Cada byte = 2 chars hex
        if (response[idx + 1] == '\0') break;  // byte incompleto
        char byte_hex[3] = {response[idx], response[idx + 1], '\0'};
        if (sscanf(byte_hex, "%hhx", &data[data_count]) != 1) {
            break;
        }
        data_count++;
        idx += 2;
    }

    if (data_count == 0) {
        return false;
    }

    out.valid = true;
    out.num_bytes = data_count;
    memcpy(out.data, data, data_count);
    out.timestamp_ms = millis();
    return true;
}

// ============================================================================
// Funções privadas — comunicação AT
// ============================================================================

static bool send_at_command(const char* cmd, uint32_t timeout_ms) {
    // Limpa buffer de recepção
    response_len = 0;
    response_buffer[0] = '\0';

    Serial.printf("[obd] >>> %s\n", cmd);

    // Envia comando + \r (carriage return é o terminador do ELM327)
    serialBT.print(cmd);
    serialBT.print('\r');

    // Aguarda resposta (terminada por '>' = prompt do ELM327)
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        while (serialBT.available()) {
            char c = serialBT.read();
            if (response_len < OBD_RESPONSE_BUFFER_SIZE - 1) {
                response_buffer[response_len++] = c;
                response_buffer[response_len] = '\0';
            }
            // '>' indica fim da resposta
            if (c == '>') {
                // Remove \r finais e o '>'
                while (response_len > 0 &&
                       (response_buffer[response_len - 1] == '\r' ||
                        response_buffer[response_len - 1] == '>')) {
                    response_buffer[--response_len] = '\0';
                }
                Serial.printf("[obd] <<< %s\n", response_buffer);
                return true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));  // Não bloqueia a CPU
    }

    // Timeout
    Serial.printf("[obd] TIMEOUT esperando resposta de %s\n", cmd);
    response_buffer[0] = '\0';
    return false;
}

static bool send_at_init_sequence() {
    // Envia cada comando AT em ordem. Falha no primeiro = falha geral.
    while (at_command_index < AT_INIT_SEQUENCE_LEN) {
        const char* cmd = AT_INIT_SEQUENCE[at_command_index];
        bool ok = send_at_command(cmd, AT_COMMAND_TIMEOUT_MS);

        // Validação específica por comando
        if (ok) {
            if (strcmp(cmd, "ATZ") == 0) {
                // ATZ responde com versão do firmware
                if (strstr(response_buffer, "ELM327") == nullptr) {
                    Serial.println("[obd] WARN: ATZ não retornou versão ELM327");
                    // Não-fatal: alguns módulos retornam strings ligeiramente diferentes
                }
            } else {
                // Demais comandos respondem "OK"
                if (strstr(response_buffer, "OK") == nullptr) {
                    Serial.printf("[obd] ERRO: %s não respondeu OK\n", cmd);
                    return false;
                }
            }
            at_command_index++;
        } else {
            // Timeout ou sem resposta
            return false;
        }
    }
    return true;  // Todos os comandos OK
}

// ============================================================================
// Funções privadas — FSM
// ============================================================================

static void fsm_pairing() {
    // Tenta conectar via Bluetooth
    Serial.println("[obd] Iniciando pairing Bluetooth...");

    bool connected;
    if (ELM327_BT_MAC != nullptr) {
        connected = serialBT.connect(ELM327_BT_MAC);
    } else {
        connected = serialBT.connect(ELM327_BT_NAME);
    }

    if (connected) {
        Serial.println("[obd] Bluetooth conectado, iniciando AT sequence");
        reset_state_machine();
        change_state(ObdClientState::INITIALIZING);
    } else {
        uint32_t elapsed = millis() - last_state_change_ms;
        if (elapsed > BT_CONNECT_TIMEOUT_MS) {
            Serial.printf("[obd] Pairing timeout após %u ms\n", elapsed);
            change_state(ObdClientState::RECOVERING);
        }
        // Caso contrário, fica tentando (SerialBT.connect retry interno)
    }
}

static void fsm_initializing() {
    if (send_at_init_sequence()) {
        Serial.println("[obd] Init AT completo, iniciando handshake");
        change_state(ObdClientState::HANDSHAKING);
    } else {
        Serial.println("[obd] Falha no init AT, indo para RECOVERING");
        change_state(ObdClientState::RECOVERING);
    }
}

static void fsm_handshaking() {
    ObdPidResponse response;
    // Usa a versão locked porque o mutex JÁ FOI tomado por obd_client_loop.
    // Usar obd_client_send aqui causaria deadlock.
    bool ok = obd_client_send_locked(ObdPid::SUPPORTED_PIDS_01_20, response);

    if (ok && response.num_bytes >= 4) {
        Serial.println("[obd] Handshake OK: ECU respondeu com bitmask válida");
        Serial.printf("[obd] Bitmask: %02X %02X %02X %02X\n",
                      response.data[0], response.data[1],
                      response.data[2], response.data[3]);
        reconnect_attempts = 0;
        change_state(ObdClientState::CONNECTED);
    } else {
        Serial.println("[obd] Handshake falhou, indo para RECOVERING");
        change_state(ObdClientState::RECOVERING);
    }
}

static void fsm_recovering() {
    uint32_t delay_ms = calculate_backoff_delay(reconnect_attempts);
    uint32_t elapsed = millis() - last_state_change_ms;

    if (elapsed >= delay_ms) {
        reconnect_attempts++;
        if (reconnect_attempts > RECONNECT_MAX_ATTEMPTS) {
            Serial.printf("[obd] WARN: %u tentativas de reconexão, falha persistente\n",
                          reconnect_attempts);
            // Não desiste — continua tentando a cada 120s
        }
        Serial.printf("[obd] Tentativa %u de reconexão após %u ms\n",
                      reconnect_attempts, delay_ms);
        serialBT.disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));
        change_state(ObdClientState::DISCONNECTED);
    }
}

// ============================================================================
// Implementação da API pública
// ============================================================================

bool obd_client_init() {
    if (obd_mutex != nullptr) {
        return true;  // Já inicializado
    }

    obd_mutex = xSemaphoreCreateMutex();
    if (obd_mutex == nullptr) {
        Serial.println("[obd] ERRO: falha ao criar mutex");
        return false;
    }

    // Inicializa BluetoothSerial como master
    serialBT.begin("ESP32_OBD_Client", true);  // true = isMaster
    Serial.println("[obd] BluetoothSerial inicializado como master");

    current_state = ObdClientState::DISCONNECTED;
    last_state_change_ms = millis();
    return true;
}

void obd_client_loop() {
    if (obd_mutex == nullptr) return;

    xSemaphoreTake(obd_mutex, portMAX_DELAY);

    uint32_t now = millis();
    // Throttle: não processa FSM mais que 1x a cada 100ms
    if (now - last_loop_call_ms < 100) {
        xSemaphoreGive(obd_mutex);
        return;
    }
    last_loop_call_ms = now;

    switch (current_state) {
        case ObdClientState::DISCONNECTED:
            // Imediatamente tenta pairing
            change_state(ObdClientState::PAIRING);
            break;

        case ObdClientState::PAIRING:
            fsm_pairing();
            break;

        case ObdClientState::INITIALIZING:
            fsm_initializing();
            break;

        case ObdClientState::HANDSHAKING:
            fsm_handshaking();
            break;

        case ObdClientState::CONNECTED:
            // Estado nominal. Em M3+, aqui vai health check periódico.
            // Por enquanto, só verifica se BT ainda conectado.
            if (!serialBT.connected()) {
                Serial.println("[obd] BT desconectou inesperadamente");
                change_state(ObdClientState::RECOVERING);
            }
            break;

        case ObdClientState::RECOVERING:
            fsm_recovering();
            break;
    }

    xSemaphoreGive(obd_mutex);
}

bool obd_client_is_connected() {
    return current_state == ObdClientState::CONNECTED;
}

ObdClientState obd_client_get_state() {
    return current_state;
}

// ============================================================================
// Função interna — assume que o mutex JÁ ESTÁ tomado pelo chamador.
// Usada pela FSM (em obd_client_loop) e por obd_client_send (wrapper público).
// ============================================================================
static bool obd_client_send_locked(ObdPid pid, ObdPidResponse& response) {
    if (current_state != ObdClientState::CONNECTED &&
        current_state != ObdClientState::HANDSHAKING) {
        return false;
    }

    // Formata comando: 2 chars hex do PID, ex.: "0105" para PID 0x05
    char cmd[8];
    snprintf(cmd, sizeof(cmd), "%02X%02X",
             0x01, static_cast<uint8_t>(pid));

    bool sent_ok = send_at_command(cmd, PID_RESPONSE_TIMEOUT_MS);
    if (!sent_ok) {
        return false;
    }

    // Verifica erros
    if (is_error_response(response_buffer)) {
        Serial.printf("[obd] Resposta de erro para PID 0x%02X: %s\n",
                      static_cast<uint8_t>(pid), response_buffer);
        return false;
    }

    // Tenta parsear como resposta de PID
    bool parsed = parse_pid_response(response_buffer, pid, response);
    if (!parsed) {
        Serial.printf("[obd] Falha ao parsear resposta: %s\n", response_buffer);
    }
    return parsed;
}

// ============================================================================
// Wrapper público — toma o mutex e chama a versão locked.
// Usado por código EXTERNO à FSM (ex.: main.cpp, futura Task_OBD).
// ============================================================================
bool obd_client_send(ObdPid pid, ObdPidResponse& response) {
    if (obd_mutex == nullptr) return false;

    xSemaphoreTake(obd_mutex, portMAX_DELAY);
    bool result = obd_client_send_locked(pid, response);
    xSemaphoreGive(obd_mutex);
    return result;
}

uint8_t obd_client_get_reconnect_attempts() {
    return reconnect_attempts;
}