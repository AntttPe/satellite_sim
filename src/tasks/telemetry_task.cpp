#include "telemetry_task.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <iostream>
#include <iomanip>

namespace Satellite {

void vTelemetryTask(void* pvParameters) {

    // ── Rozpakuj parametry ze struktury ──────────────────
    // static_cast zamiast reinterpret_cast — bezpieczniejsze,
    // kompilator sprawdza czy typy są kompatybilne.
    // reinterpret_cast = "ufaj mi, wiem co robię" — używaj tylko
    // gdy MUSISZ (np. hardware registers). Tu mamy zwykły void*.
    TelemetryTaskParams* params = static_cast<TelemetryTaskParams*>(pvParameters);
    QueueHandle_t sensor_queue  = params->sensor_queue;
    QueueHandle_t orbit_queue   = params->orbit_queue;
    QueueHandle_t laser_queue   = params->laser_queue;

    std::cout << "[TELEM] Task wystartował" << std::endl;

    uint32_t packet_id = 0;

    // ── Bufory na dane z kolejek ──────────────────────────
    // Każda struktura żyje na stosie taska (512 słów = ok 2KB na arm64)
    // Łączny rozmiar buforów: SensorData + OrbitData + LaserLinkData
    // to ok. 100 bajtów — bezpiecznie mieści się na stosie
    SensorData   sensor;
    OrbitData    orbit;
    LaserLinkData laser;

    // Flagi — czy mamy już dane z każdego źródła
    // Zaczynamy bez danych, czekamy aż wszystkie taski wyślą pierwsze pakiety
    bool has_orbit = false;
    bool has_laser = false;

    while (true) {

        // ── Czekaj na dane z SensorTask (blokująco) ──────
        // portMAX_DELAY = task śpi aż coś przyjdzie — zero CPU waste.
        // To GŁÓWNE źródło — takt całego TelemetryTask.
        // SensorTask wysyła co 100ms → TelemetryTask budzi się co 100ms.
        BaseType_t received = xQueueReceive(sensor_queue, &sensor, portMAX_DELAY);
        if (received != pdTRUE) continue;

        // ── Nieblokująco sprawdź orbit_queue ─────────────
        // 0 jako timeout = "sprawdź czy jest coś teraz, jeśli nie — idź dalej"
        // Nie chcemy CZEKAĆ na orbit (update co 1s) bo zablokowałoby
        // cały TelemetryTask na 1s zamiast działać co 100ms.
        //
        // WAŻNA LEKCJA: xQueueReceive z timeout=0 to "peek bez blokowania"
        // Używaj gdy dane są opcjonalne lub mają inną częstotliwość.
        if (xQueueReceive(orbit_queue, &orbit, 0) == pdTRUE) {
            has_orbit = true;
        }

        // ── Nieblokująco sprawdź laser_queue ─────────────
        if (xQueueReceive(laser_queue, &laser, 0) == pdTRUE) {
            has_laser = true;
        }

        ++packet_id;

        // ── Status systemu ────────────────────────────────
        const char* status_str =
            (sensor.temperature > 70.0f)  ? "WARNING-TEMP" :
            (sensor.temperature < -30.0f) ? "WARNING-COLD" :
            (has_laser && !laser.link_active) ? "WARNING-LASER" :
                                               "OK";

        // ── Drukuj co 10 pakietów orbit + laser ──────────
        // Sensor wysyła co 100ms = 10 razy na sekundę.
        // Orbit update co 1s — drukujemy go przy każdym 10. pakiecie
        // żeby nie zaśmiecać konsoli ale pokazać że działa.
        bool print_full = (packet_id % 10 == 0);

        std::cout << "\n[TELEM] ═══ Packet #" << packet_id << " ═══\n";
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "  Timestamp : " << sensor.timestamp_ms << " ms\n";
        std::cout << "  Status    : " << status_str           << "\n";
        std::cout << "  Temp      : " << sensor.temperature   << " °C\n";
        std::cout << "  Gyro  XYZ : "
                  << sensor.gyro_x  << " | "
                  << sensor.gyro_y  << " | "
                  << sensor.gyro_z  << " deg/s\n";
        std::cout << "  Accel XYZ : "
                  << sensor.accel_x << " | "
                  << sensor.accel_y << " | "
                  << sensor.accel_z << " m/s²\n";

        if (print_full && has_orbit) {
            std::cout << std::fixed << std::setprecision(4);
            std::cout << "  ── Orbit ──────────────────────────\n";
            std::cout << "  Lat/Lon   : "
                      << orbit.latitude  << "° / "
                      << orbit.longitude << "°\n";
            std::cout << "  Altitude  : "
                      << std::setprecision(1) << orbit.altitude_km << " km\n";
            std::cout << "  Velocity  : "
                      << (int)orbit.velocity_ms << " m/s\n";
            std::cout << "  Anomaly   : "
                      << std::setprecision(2) << orbit.true_anomaly << "°\n";
        }

        if (print_full && has_laser) {
            std::cout << "  ── Laser Link ─────────────────────\n";
            std::cout << "  Link      : "
                      << (laser.link_active ? "ACTIVE" : "DOWN") << "\n";
            if (laser.link_active) {
                std::cout << "  Signal    : "
                          << std::setprecision(1) << laser.signal_strength << " dBm\n";
                std::cout << "  Atm.Loss  : "
                          << laser.atmospheric_loss << " dB\n";
                std::cout << "  Ping      : " << laser.ping_ms << " ms\n";

                // ── Formatowanie BER w notacji naukowej ──
                // BER to bardzo mała liczba (np. 1e-9)
                // printf z %e daje notację naukową: 1.000e-09
                // cout nie ma prostego odpowiednika — printf jest tu czytelniejszy
                printf("  BER       : %.3e\n", laser.bit_error_rate);
            }
        }
    }
}

} // namespace Satellite