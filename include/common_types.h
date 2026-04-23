#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <cstdint>    // uint32_t, uint8_t itd.

namespace Satellite {

    // ── Dane z czujników IMU + temperatura ──────────────
    // IMU = Inertial Measurement Unit — żyroskop + akcelerometr
    struct SensorData {
        // Żyroskop — prędkość kątowa [stopnie/sekundę]
        float gyro_x;
        float gyro_y;
        float gyro_z;

        // Akcelerometr — przyspieszenie [m/s²]
        float accel_x;
        float accel_y;
        float accel_z;

        // Temperatura panelu słonecznego [°C]
        float temperature;

        // Kiedy zmierzono — tick schedulera
        uint32_t timestamp_ms;
    };

    // ── Packet telemetryczny wysyłany do "naziemnej stacji" ──
    struct TelemetryPacket {
        uint32_t packet_id;        // numer pakietu (rośnie)
        uint32_t timestamp_ms;     // czas wysłania
        SensorData sensor_data;    // zagnieżdżona struktura!
        float battery_voltage;     // napięcie baterii [V]
        uint8_t system_status;     // 0=OK, 1=WARNING, 2=ERROR
    };

    // ── Stałe misji ──────────────────────────────────────
    // constexpr = wartość znana w czasie kompilacji
    // (nowoczesna alternatywa dla #define)
    constexpr uint32_t SENSOR_TASK_PERIOD_MS    = 100;   // pomiar co 100ms
    constexpr uint32_t TELEMETRY_TASK_PERIOD_MS = 1000;  // wysyłka co 1s
    constexpr uint32_t WATCHDOG_TIMEOUT_MS      = 5000;  // 5s bez sygnału = błąd

    constexpr float    BATTERY_NOMINAL_V        = 8.4f;  // pełna bateria
    constexpr float    BATTERY_CRITICAL_V       = 6.0f;  // poziom krytyczny

} // namespace Satellite

#endif // COMMON_TYPES_H