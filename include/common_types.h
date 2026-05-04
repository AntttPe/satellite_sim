#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <cstdint>    // uint32_t, uint8_t itd.
#include <sys/types.h>

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


    // uroszczony model orbity, w nastpnych projektach implemetnacja SGP4 + TLE
    struct OrbitData {
        double latitude; // szerokosc geograficzna [-90, 90]
        double longitude; // dlugosc geograficzna [-180, 180]
        double altitude_km; // wysokosc nad powieszchnia ziemi
        double velocity_ms; // predkosc orbitalna [m/s]
        double inclination; // iknlinacja orbity [stopnie] - kat do rownika
        double true_anomaly; // kat aktualnej pozycji na orbicie [stopnie]
        uint32_t timestamp_ms;
    };

    // dane łącza laserowego:
    struct LaserLinkData {
        bool link_active; //czy łącze jest aktywne
        float signal_strength; // moc sygnału dBm
        float bit_error_rate; // BER - ile bitow jest błędnych (0.0 - 1.0)
        float atmospheric_loss;  // tlumienie atmosferyczne
        uint32_t ping_ms;          // opoznienie [ms]
        uint32_t timestamp_ms;
    };

    // Warunki srodowskowe - wysylane z Sim Control
    // Ta struktura jest wejsciem do symulaotra z zewnątrz
    struct SimConditions {
        float cloud_coverage; // zachmurzenie (0.0 - 1.0)
        float atmospheric_scint; //scyntylacja powietrza, rozpraszajaca laser - turbylencja powietrza rozpraszajace laser
        float solar_irradiance; // naslonecznienie [w/m2], max 1361 W/m2
        bool eclipse; // czy satelia jest w cieniu ziemi
        float orbit_altitude_km; // zadana wysokosc orbity
        float orbit_inclination; // zadana inklinacja
    };

    // Packet telemetryczny wysyłany do "naziemnej stacji"
    struct TelemetryPacketV2 {
        uint32_t packet_id;
        uint32_t timestamp_ms;
        SensorData sensor;
        OrbitData orbit;
        LaserLinkData lase;
        float battery_voltage;
        uint8_t system_status; // 0 = OK, 1 = WARRING, 2 = ERROR
    };

    // Stałe misji
    // constexpr = wartość znana w czasie kompilacj (nowoczesna alternatywa dla #define)
    constexpr uint32_t SENSOR_TASK_PERIOD_MS    = 100;   // pomiar co 100ms
    constexpr uint32_t TELEMETRY_TASK_PERIOD_MS = 1000;  // wysyłka co 1s
    constexpr uint32_t WATCHDOG_TIMEOUT_MS      = 5000;  // 5s bez sygnału = błąd

    constexpr float    BATTERY_NOMINAL_V        = 8.4f;  // pełna bateria
    constexpr float    BATTERY_CRITICAL_V       = 6.0f;  // poziom krytyczny

    constexpr double EARTH_RADIUS_KM    = 6371.0;
    constexpr double MU_EARTH = 3.986e14; // Stala grawitacyjna
    constexpr double PI = 3.14159265358979;
    constexpr double DEG_TO_RAD = PI / 180.0;
    constexpr double RAD_TO_DEG = 180.0 / PI;
    constexpr uint32_t ORBIT_TASK_PERIOD_MS = 1000; // update orbity co 1s
    constexpr uint32_t LASER_TASK_PERIOD_MS = 500; // update lasera co 0.5 s
} // namespace Satellite


#endif // COMMON_TYPES_H