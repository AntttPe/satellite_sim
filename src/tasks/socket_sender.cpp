#include "socket_sender.h"
#include "task.h"
#include "queue.h"

#include <iostream>
#include <sstream>    // std::ostringstream — budowanie stringa
#include <iomanip>    // std::fixed, std::setprecision
#include <cstring>    // memset

// ── Sockety POSIX — Unix/macOS/Linux API ─────────────────
// To niskopoziomowe API systemu operacyjnego.
// W prawdziwym satelicie byłoby to UART lub CAN bus,
// tu używamy TCP socket do komunikacji z Pythonem.
// <sys/socket.h> = funkcje socket(), connect(), send()
// <netinet/in.h> = struct sockaddr_in, IPPROTO_TCP
// <arpa/inet.h>  = inet_pton() — konwersja IP string → binarny
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>    // close()
#include <errno.h>     // errno, strerror()

namespace Satellite {

// ── Buduj JSON z danych telemetrycznych ──────────────────
// ostringstream = string builder — wydajniejszy niż + na stringach
// Każde + na std::string tworzy nową kopię (O(n)).
// ostringstream buforuje i tworzy string raz na końcu (O(1) amortized).
std::string buildTelemetryJson(
    uint32_t packet_id,
    const SensorData&    sensor,
    const OrbitData&     orbit,
    const LaserLinkData& laser)
{
    std::ostringstream j;

    // std::fixed + setprecision = zawsze N miejsc po przecinku
    // Bez tego float może dać "9.8" albo "9.819999694824" — nieprzewidywalne
    j << std::fixed << std::setprecision(4);

    j << "{"
      // ── Nagłówek pakietu ───────────────────────────────
      << "\"packet_id\":"    << packet_id           << ","
      << "\"timestamp_ms\":" << sensor.timestamp_ms << ","

      // ── Dane sensorów ──────────────────────────────────
      << "\"sensor\":{"
        << "\"gyro_x\":"       << sensor.gyro_x       << ","
        << "\"gyro_y\":"       << sensor.gyro_y       << ","
        << "\"gyro_z\":"       << sensor.gyro_z       << ","
        << "\"accel_x\":"      << sensor.accel_x      << ","
        << "\"accel_y\":"      << sensor.accel_y      << ","
        << "\"accel_z\":"      << sensor.accel_z      << ","
        << "\"temperature\":"  << sensor.temperature  << ","
        << "\"timestamp_ms\":" << sensor.timestamp_ms
      << "},"

      // ── Dane orbitalne ─────────────────────────────────
      << "\"orbit\":{"
        << "\"latitude\":"     << orbit.latitude      << ","
        << "\"longitude\":"    << orbit.longitude     << ","
        << "\"altitude_km\":"  << orbit.altitude_km   << ","
        << "\"velocity_ms\":"  << orbit.velocity_ms   << ","
        << "\"inclination\":"  << orbit.inclination   << ","
        << "\"true_anomaly\":" << orbit.true_anomaly  << ","
        << "\"timestamp_ms\":" << orbit.timestamp_ms
      << "},"

      // ── Dane lasera ────────────────────────────────────
      // bool w JSON = true/false (małe litery!) nie "true"/"false"
      // std::boolalpha mówi streamowi żeby pisał true/false zamiast 1/0
      << "\"laser\":{"
        << "\"link_active\":"      << std::boolalpha
                                   << laser.link_active  << std::noboolalpha << ","
        << "\"signal_strength\":"  << laser.signal_strength  << ","
        << "\"bit_error_rate\":"   << std::scientific << std::setprecision(3)
                                   << laser.bit_error_rate
                                   << std::fixed << std::setprecision(4) << ","
        << "\"atmospheric_loss\":" << laser.atmospheric_loss << ","
        << "\"ping_ms\":"          << laser.ping_ms          << ","
        << "\"timestamp_ms\":"     << laser.timestamp_ms
      << "},"

      // ── Dodatkowe pola pakietu ─────────────────────────
      << "\"battery_voltage\":8.2,"   // TODO: dodać BatteryTask
      << "\"system_status\":0"        // 0=OK
    << "}";

    // \n na końcu — Python readline() czeka na newline!
    // Bez \n Python nigdy nie dostanie kompletnej linii.
    return j.str() + "\n";
}

// ── Główny task wysyłający przez socket ──────────────────
void vSocketSenderTask(void* pvParameters) {
    SocketSenderParams* params = static_cast<SocketSenderParams*>(pvParameters);

    // Lokalne bufory na ostatnie odebrane dane
    SensorData    sensor = {};   // {} = zero-initialize wszystkie pola
    OrbitData     orbit  = {};   // ważne! bez tego struct ma śmieciowe wartości
    LaserLinkData laser  = {};

    bool has_sensor = false;
    bool has_orbit  = false;
    bool has_laser  = false;

    uint32_t packet_id   = 0;
    int      sock_fd     = -1;   // -1 = socket nie otwarty
    bool     connected   = false;

    std::cout << "[SOCKET] Task wystartował, łączę z Python bridge...\n";

    // ── PĘTLA GŁÓWNA ──────────────────────────────────────
    for (;;) {
        // ── Krok 1: Upewnij się że mamy połączenie ────────
        // Jeśli socket nie istnieje lub zerwał się — reconnect.
        // To ważny pattern: auto-reconnect zamiast crash przy utracie połączenia.
        if (!connected) {
            // Zamknij stary socket jeśli był otwarty
            if (sock_fd >= 0) {
                close(sock_fd);
                sock_fd = -1;
            }

            // socket(AF_INET, SOCK_STREAM, 0) = stwórz TCP socket
            // AF_INET    = IPv4 (vs AF_INET6 dla IPv6)
            // SOCK_STREAM = TCP (vs SOCK_DGRAM dla UDP)
            sock_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (sock_fd < 0) {
                std::cerr << "[SOCKET] Błąd tworzenia socketu: "
                          << strerror(errno) << "\n";
                vTaskDelay(pdMS_TO_TICKS(3000)); // czekaj 3s przed retry
                continue;
            }

            // ── Adres serwera (Python FastAPI bridge) ─────
            struct sockaddr_in server_addr;
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port   = htons(9000); // htons = host to network byte order
                                                   // sieć używa big-endian, CPU może być little-endian
                                                   // htons konwertuje poprawnie na każdej architekturze!

            // inet_pton = "presentation to network"
            // konwertuje string "127.0.0.1" → binarny adres IP
            inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

            // connect() = nawiąż połączenie TCP (three-way handshake)
            int result = connect(sock_fd,
                                 (struct sockaddr*)&server_addr,
                                 sizeof(server_addr));
            if (result < 0) {
                std::cerr << "[SOCKET] Nie mogę połączyć z Python bridge: "
                          << strerror(errno) << " — retry za 3s\n";
                close(sock_fd);
                sock_fd = -1;
                vTaskDelay(pdMS_TO_TICKS(3000));
                continue;
            }

            connected = true;
            std::cout << "[SOCKET] Połączono z Python bridge (127.0.0.1:9000)\n";
        }

        // ── Krok 2: Zbierz dane ze wszystkich kolejek ─────
        // Nieblokująco (timeout=0) — nie czekamy, bierzemy co jest
        if (xQueueReceive(params->sensor_queue, &sensor, 0) == pdTRUE) {
            has_sensor = true;
        }
        if (xQueueReceive(params->orbit_queue,  &orbit,  0) == pdTRUE) {
            has_orbit = true;
        }
        if (xQueueReceive(params->laser_queue,  &laser,  0) == pdTRUE) {
            has_laser = true;
        }

        // Wyślij tylko gdy mamy dane ze wszystkich źródeł
        if (!has_sensor || !has_orbit || !has_laser) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // ── Krok 3: Zbuduj i wyślij JSON ──────────────────
        ++packet_id;
        std::string json = buildTelemetryJson(packet_id, sensor, orbit, laser);

        // send() = wyślij bajty przez socket
        // c_str() = konwertuje std::string na const char* (C-style string)
        // json.size() = liczba bajtów do wysłania
        ssize_t sent = send(sock_fd, json.c_str(), json.size(), 0);

        if (sent < 0) {
            std::cerr << "[SOCKET] Błąd wysyłania: "
                      << strerror(errno) << " — reconnect\n";
            connected = false;  // wymusi reconnect w następnej iteracji
            continue;
        }

        if (packet_id % 10 == 0) {
            std::cout << "[SOCKET] Wysłano pakiet #" << packet_id
                      << " (" << sent << " bajtów)\n";
        }

        // Wysyłaj co 1 sekundę — synchronizacja z OrbitTask
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

} // namespace Satellite