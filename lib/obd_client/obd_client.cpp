/**
 * @file obd_client.cpp
 * @brief Implementação do cliente OBD-II (Bluetooth SPP + ELM327 V1.5).
 *
 * M3 — Adiciona:
 *   - Funções de conversão SAE J1979 para 9 PIDs.
 *   - Cache de bitmask de PIDs suportados.
 *   - Leitura de snapshot com sampling diferenciado.
 *   - Métricas de latência por PID.
 *
 * @author Gabriel Davi Silva Resende (PET-TEC UNIFEI)
 * @license MIT
 * @version 0.2.0
 */

#include "obd_client.h"

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <freertos/semphr.h>
#include <cstring>
#include <cstdio>
#include <cmath>

#include "../../include/obd_config.h"
#include "../../include/obd_pids.h"

// ============================================================================
// Forward declarations
// ============================================================================
static bool obd_client_send_locked(ObdPid pid, ObdPidResponse& response);

// ============================================================================
// Estado interno
// ============================================================================
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

// Cache de PIDs suportados (bitmask de 4 bytes do PID 0x00)
static bool     bitmask_cached = false;
static uint8_t  supported_bitmask[4] = {0, 0, 0, 0};

// ============================================================================
// Sequência de comandos AT de inicialização
// ============================================================================
static const char* AT_INIT_SEQUENCE[] = {
    "ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP0",
};
static const uint8_t AT_INIT_SEQUENCE_LEN = 
    sizeof(AT_INIT_SEQUENCE) / sizeof(AT_INIT_SEQUENCE[0]);

// ============================================================================
// Funções utilitárias
// ============================================================================

static uint32_t calculate_backoff_delay(uint8_t attempt) {
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
    bitmask_cached = false;
    serialBT.flush();
}

// ============================================================================
// Parser de resposta
// ============================================================================

static bool is_error_response(const char* response) {
    if (response == nullptr) return false;
    return strncmp(response, "NO DATA", 7) == 0 ||
           strncmp(response, "UNABLE TO CONNECT", 18) == 0 ||
           strncmp(response, "BUS INIT", 8) == 0 ||
           strncmp(response, "DATA ERROR", 10) == 0 ||
           strcmp(response, "?") == 0;
}

[[maybe_unused]]
static bool is_searching(const char* response) {
    return strstr(response, "SEARCHING") != nullptr;
}

static bool parse_pid_response(const char* response, ObdPid expected_pid,
                                ObdPidResponse& out) {
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
        if (response[idx + 1] == '\0') break;
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
// Funções de conversão SAE J1979
// ============================================================================
// Cada função valida o número de bytes e aplica a fórmula canônica.

bool convert_ect(const ObdPidResponse& resp, int8_t& out_celsius) {
    if (!resp.valid || resp.num_bytes < 1) return false;
    // ECT = A - 40, range -40 a 215°C
    out_celsius = static_cast<int8_t>(resp.data[0]) - 40;
    return true;
}

bool convert_stft(const ObdPidResponse& resp, int8_t& out_percent) {
    if (!resp.valid || resp.num_bytes < 1) return false;
    // STFT = (A - 128) * 100/128, range -100 a +99.2%
    int16_t value = (static_cast<int16_t>(resp.data[0]) - 128) * 100 / 128;
    out_percent = static_cast<int8_t>(value);
    return true;
}

bool convert_ltft(const ObdPidResponse& resp, int8_t& out_percent) {
    if (!resp.valid || resp.num_bytes < 1) return false;
    // LTFT = (A - 128) * 100/128 (mesma fórmula do STFT)
    int16_t value = (static_cast<int16_t>(resp.data[0]) - 128) * 100 / 128;
    out_percent = static_cast<int8_t>(value);
    return true;
}

bool convert_rpm(const ObdPidResponse& resp, uint16_t& out_rpm) {
    if (!resp.valid || resp.num_bytes < 2) return false;
    // RPM = ((A*256) + B) / 4, range 0 a 16383.75
    uint16_t raw = (static_cast<uint16_t>(resp.data[0]) << 8) | resp.data[1];
    out_rpm = raw / 4;
    return true;
}

bool convert_speed(const ObdPidResponse& resp, uint8_t& out_kmh) {
    if (!resp.valid || resp.num_bytes < 1) return false;
    // Speed = A km/h, range 0-255
    out_kmh = resp.data[0];
    return true;
}

bool convert_iat(const ObdPidResponse& resp, int8_t& out_celsius) {
    if (!resp.valid || resp.num_bytes < 1) return false;
    // IAT = A - 40 (mesma fórmula do ECT)
    out_celsius = static_cast<int8_t>(resp.data[0]) - 40;
    return true;
}

bool convert_throttle(const ObdPidResponse& resp, uint8_t& out_percent) {
    if (!resp.valid || resp.num_bytes < 1) return false;
    // TP = A * 100/255, range 0-100%
    out_percent = (static_cast<uint16_t>(resp.data[0]) * 100) / 255;
    return true;
}

bool convert_o2_voltage(const ObdPidResponse& resp, float& out_volts) {
    if (!resp.valid || resp.num_bytes < 2) return false;
    // O2 V = A * 0.005, range 0-1.275 V
    // (B é o fuel trim do sensor, lido separadamente por convert_o2_trim)
    out_volts = resp.data[0] * 0.005f;
    return true;
}

bool convert_o2_trim(const ObdPidResponse& resp, int8_t& out_percent) {
    if (!resp.valid || resp.num_bytes < 2) return false;
    // O2 trim = (B - 128) * 100/128 (mesma fórmula do STFT, mas usa byte B)
    int16_t value = (static_cast<int16_t>(resp.data[1]) - 128) * 100 / 128;
    out_percent = static_cast<int8_t>(value);
    return true;
}

// ============================================================================
// Verificação de suporte (bitmask)
// ============================================================================

bool is_pid_supported(ObdPid pid, const uint8_t bitmask[4]) {
    uint8_t pid_value = static_cast<uint8_t>(pid);
    if (pid_value < 0x01 || pid_value > 0x20) {
        return false;  // Fora do range da bitmask 01-20
    }

    // Mapeamento SAE J1979 padrão:
    // 4 bytes: A (0x01..0x08), B (0x09..0x10), C (0x11..0x18), D (0x19..0x20)
    // Bit 7 (MSB) é o primeiro PID do byte, bit 0 (LSB) é o último
    uint8_t byte_index = (pid_value - 1) / 8;
    uint8_t bit_offset = 7 - ((pid_value - 1) % 8);

    return (bitmask[byte_index] & (1 << bit_offset)) != 0;
}

// ============================================================================
// Comunicação AT
// ============================================================================

static bool send_at_command(const char* cmd, uint32_t timeout_ms) {
    response_len = 0;
    response_buffer[0] = '\0';

    Serial.printf("[obd] >>> %s\n", cmd);

    serialBT.print(cmd);
    serialBT.print('\r');

    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        while (serialBT.available()) {
            char c = serialBT.read();
            if (response_len < OBD_RESPONSE_BUFFER_SIZE - 1) {
                response_buffer[response_len++] = c;
                response_buffer[response_len] = '\0';
            }
            if (c == '>') {
                while (response_len > 0 &&
                       (response_buffer[response_len - 1] == '\r' ||
                        response_buffer[response_len - 1] == '>')) {
                    response_buffer[--response_len] = '\0';
                }
                Serial.printf("[obd] <<< %s\n", response_buffer);
                return true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    Serial.printf("[obd] TIMEOUT esperando resposta de %s\n", cmd);
    response_buffer[0] = '\0';
    return false;
}

static bool send_at_init_sequence() {
    while (at_command_index < AT_INIT_SEQUENCE_LEN) {
        const char* cmd = AT_INIT_SEQUENCE[at_command_index];
        bool ok = send_at_command(cmd, AT_COMMAND_TIMEOUT_MS);

        if (ok) {
            if (strcmp(cmd, "ATZ") == 0) {
                if (strstr(response_buffer, "ELM327") == nullptr) {
                    Serial.println("[obd] WARN: ATZ não retornou versão ELM327");
                }
            } else {
                if (strstr(response_buffer, "OK") == nullptr) {
                    Serial.printf("[obd] ERRO: %s não respondeu OK\n", cmd);
                    return false;
                }
            }
            at_command_index++;
        } else {
            return false;
        }
    }
    return true;
}

// ============================================================================
// FSM
// ============================================================================

static void fsm_pairing() {
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
    bool ok = obd_client_send_locked(ObdPid::SUPPORTED_PIDS_01_20, response);

    if (ok && response.num_bytes >= 4) {
        // Armazena a bitmask no cache
        memcpy(supported_bitmask, response.data, 4);
        bitmask_cached = true;

        Serial.println("[obd] Handshake OK: ECU respondeu com bitmask válida");
        Serial.printf("[obd] Bitmask: %02X %02X %02X %02X\n",
                      supported_bitmask[0], supported_bitmask[1],
                      supported_bitmask[2], supported_bitmask[3]);

        // Log de quais PIDs do projeto são suportados
        Serial.print("[obd] PIDs suportados do projeto: ");
        uint8_t count = 0;
        ObdPid project_pids[] = {
            ObdPid::ECT, ObdPid::STFT, ObdPid::LTFT, ObdPid::RPM,
            ObdPid::SPEED, ObdPid::IAT, ObdPid::THROTTLE_POS,
            ObdPid::O2_SENSORS_PRESENT, ObdPid::O2_SENSOR_2
        };
        for (ObdPid p : project_pids) {
            if (is_pid_supported(p, supported_bitmask)) {
                Serial.printf("%s ", obd_pid_name(p));
                count++;
            }
        }
        Serial.printf("(%u/9)\n", count);

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
            Serial.printf("[obd] WARN: %u tentativas, falha persistente\n",
                          reconnect_attempts);
        }
        Serial.printf("[obd] Tentativa %u de reconexão após %u ms\n",
                      reconnect_attempts, delay_ms);
        serialBT.disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));
        change_state(ObdClientState::DISCONNECTED);
    }
}

// ============================================================================
// API pública — conexão
// ============================================================================

bool obd_client_init() {
    if (obd_mutex != nullptr) {
        return true;
    }

    obd_mutex = xSemaphoreCreateMutex();
    if (obd_mutex == nullptr) {
        Serial.println("[obd] ERRO: falha ao criar mutex");
        return false;
    }

    serialBT.begin("ESP32_OBD_Client", true);
    Serial.println("[obd] BluetoothSerial inicializado como master");

    current_state = ObdClientState::DISCONNECTED;
    last_state_change_ms = millis();
    return true;
}

void obd_client_loop() {
    if (obd_mutex == nullptr) return;

    xSemaphoreTake(obd_mutex, portMAX_DELAY);

    uint32_t now = millis();
    if (now - last_loop_call_ms < 100) {
        xSemaphoreGive(obd_mutex);
        return;
    }
    last_loop_call_ms = now;

    switch (current_state) {
        case ObdClientState::DISCONNECTED:
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

uint8_t obd_client_get_reconnect_attempts() {
    return reconnect_attempts;
}

// ============================================================================
// API pública — leitura de PIDs (M3)
// ============================================================================

static bool obd_client_send_locked(ObdPid pid, ObdPidResponse& response) {
    if (current_state != ObdClientState::CONNECTED &&
        current_state != ObdClientState::HANDSHAKING) {
        return false;
    }

    char cmd[8];
    snprintf(cmd, sizeof(cmd), "%02X%02X",
             0x01, static_cast<uint8_t>(pid));

    uint32_t request_start = millis();
    bool sent_ok = send_at_command(cmd, PID_RESPONSE_TIMEOUT_MS);
    uint32_t latency = millis() - request_start;

    response.latency_ms = latency;

    if (!sent_ok) {
        return false;
    }

    if (is_error_response(response_buffer)) {
        Serial.printf("[obd] Erro PID 0x%02X: %s\n",
                      static_cast<uint8_t>(pid), response_buffer);
        return false;
    }

    bool parsed = parse_pid_response(response_buffer, pid, response);
    if (!parsed) {
        Serial.printf("[obd] Parse falhou PID 0x%02X: %s\n",
                      static_cast<uint8_t>(pid), response_buffer);
    }
    return parsed;
}

bool obd_client_send(ObdPid pid, ObdPidResponse& response) {
    if (obd_mutex == nullptr) return false;

    xSemaphoreTake(obd_mutex, portMAX_DELAY);
    bool result = obd_client_send_locked(pid, response);
    xSemaphoreGive(obd_mutex);
    return result;
}

bool obd_client_is_pid_supported(ObdPid pid) {
    if (!bitmask_cached) return false;
    return is_pid_supported(pid, supported_bitmask);
}

uint8_t obd_client_count_supported_pids() {
    if (!bitmask_cached) return 0;

    ObdPid project_pids[] = {
        ObdPid::ECT, ObdPid::STFT, ObdPid::LTFT, ObdPid::RPM,
        ObdPid::SPEED, ObdPid::IAT, ObdPid::THROTTLE_POS,
        ObdPid::O2_SENSORS_PRESENT, ObdPid::O2_SENSOR_2
    };

    uint8_t count = 0;
    for (ObdPid p : project_pids) {
        if (is_pid_supported(p, supported_bitmask)) {
            count++;
        }
    }
    return count;
}

bool obd_client_read_all_supported_pids(ObdSnapshot& snapshot) {
    if (obd_mutex == nullptr) return false;
    if (current_state != ObdClientState::CONNECTED) return false;

    snapshot.timestamp_ms = millis();
    snapshot.pids_attempted = 0;
    snapshot.pids_succeeded = 0;

    xSemaphoreTake(obd_mutex, portMAX_DELAY);

    uint32_t cycle_start = millis();
    uint8_t  attempted = 0;
    uint8_t  succeeded = 0;

    // Contador estático para sampling diferenciado (lento/eventual)
    static uint8_t cycle_counter = 0;
    cycle_counter++;
    bool read_slow_pids = (cycle_counter % 5 == 0);   // 0.2 Hz
    bool read_eventual_pids = (cycle_counter % 2 == 0); // 0.5 Hz

    ObdPidResponse resp;

    // ---- PIDs rápidos + médios (sempre lidos) ----

    if (is_pid_supported(ObdPid::RPM, supported_bitmask)) {
        attempted++;
        if (obd_client_send_locked(ObdPid::RPM, resp)) {
            if (convert_rpm(resp, snapshot.rpm)) {
                snapshot.rpm_valid = true;
                succeeded++;
            }
        }
    }

    if (is_pid_supported(ObdPid::SPEED, supported_bitmask)) {
        attempted++;
        if (obd_client_send_locked(ObdPid::SPEED, resp)) {
            if (convert_speed(resp, snapshot.speed_kmh)) {
                snapshot.speed_valid = true;
                succeeded++;
            }
        }
    }

    if (is_pid_supported(ObdPid::THROTTLE_POS, supported_bitmask)) {
        attempted++;
        if (obd_client_send_locked(ObdPid::THROTTLE_POS, resp)) {
            if (convert_throttle(resp, snapshot.throttle_percent)) {
                snapshot.throttle_valid = true;
                succeeded++;
            }
        }
    }

    if (is_pid_supported(ObdPid::STFT, supported_bitmask)) {
        attempted++;
        if (obd_client_send_locked(ObdPid::STFT, resp)) {
            if (convert_stft(resp, snapshot.stft_percent)) {
                snapshot.stft_valid = true;
                succeeded++;
            }
        }
    }

    if (is_pid_supported(ObdPid::LTFT, supported_bitmask)) {
        attempted++;
        if (obd_client_send_locked(ObdPid::LTFT, resp)) {
            if (convert_ltft(resp, snapshot.ltft_percent)) {
                snapshot.ltft_valid = true;
                succeeded++;
            }
        }
    }

    // ---- PIDs lentos (a cada 5 ciclos) ----

    if (read_slow_pids) {
        if (is_pid_supported(ObdPid::ECT, supported_bitmask)) {
            attempted++;
            if (obd_client_send_locked(ObdPid::ECT, resp)) {
                if (convert_ect(resp, snapshot.ect_celsius)) {
                    snapshot.ect_valid = true;
                    succeeded++;
                }
            }
        }

        if (is_pid_supported(ObdPid::IAT, supported_bitmask)) {
            attempted++;
            if (obd_client_send_locked(ObdPid::IAT, resp)) {
                if (convert_iat(resp, snapshot.iat_celsius)) {
                    snapshot.iat_valid = true;
                    succeeded++;
                }
            }
        }
    }

    // ---- PIDs eventuais (a cada 2 ciclos) ----

    if (read_eventual_pids) {
        if (is_pid_supported(ObdPid::O2_SENSOR_2, supported_bitmask)) {
            attempted++;
            if (obd_client_send_locked(ObdPid::O2_SENSOR_2, resp)) {
                if (convert_o2_voltage(resp, snapshot.o2_voltage)) {
                    snapshot.o2_v_valid = true;
                    succeeded++;
                }
                if (convert_o2_trim(resp, snapshot.o2_trim_percent)) {
                    snapshot.o2_trim_valid = true;
                }
            }
        }
    }

    snapshot.pids_attempted = attempted;
    snapshot.pids_succeeded = succeeded;
    snapshot.cycle_latency_ms = millis() - cycle_start;

    xSemaphoreGive(obd_mutex);

    Serial.printf("[obd] ciclo: %u/%u PIDs OK, latência %u ms\n",
                  succeeded, attempted, snapshot.cycle_latency_ms);

    return (succeeded > 0);
}