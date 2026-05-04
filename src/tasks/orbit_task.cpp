#include "orbit_task.h"
#include "task.h"         // vTaskDelay
#include "queue.h"        // xQueueSend
#include <cmath>          // sin, cos, sqrt, fmod
#include <iostream>

// ── WAŻNA LEKCJA: zmienne globalne w C++ ─────────────────
// g_sim_conditions to zmienna globalna — czytają ją wiele tasków.
// W prawdziwym systemie użylibyśmy mutexu (xSemaphoreCreateMutex)
// żeby chronić dostęp. Tu dla uproszczenia (wartości są atomowe)
// pomijamy mutex — zapamiętaj że w produkcji to byłby błąd!
namespace Satellite {

SimConditions g_sim_conditions = {
    .cloud_coverage    = 0.2f,   // domyślnie 20% zachmurzenia
    .atmospheric_scint = 0.1f,
    .solar_irradiance  = 1361.0f,
    .eclipse           = false,
    .orbit_altitude_km = 550.0f, // typowa orbita LEO (jak Starlink)
    .orbit_inclination = 53.0f   // inklinacja Starlink
};

// ── Prawo Keplera: prędkość orbitalna ────────────────────
// v = sqrt(μ / r) gdzie:
//   μ = stała grawitacyjna Ziemi (GM) = 3.986 × 10¹⁴ m³/s²
//   r = promień orbity = promień Ziemi + wysokość [metry]
// Dla ISS (408km): ~7660 m/s
// Dla Starlink (550km): ~7600 m/s
double calculateOrbitalVelocity(double altitude_km) {
    double r = (EARTH_RADIUS_KM + altitude_km) * 1000.0; // km → m
    return std::sqrt(MU_EARTH / r);
}

// ── Przeliczenie anomalii prawdziwej → lat/lon ───────────
// To uproszczony model — prawdziwy wymaga macierzy rotacji 3D.
// Inklinacja sprawia że tor lotu jest "przechylony" względem równika.
//
// MECHANIKA:
//   lat = arcsin(sin(inklinacja) * sin(anomalia))
//   lon = anomalia + offset_czasowy  (Ziemia obraca się pod satelitą!)
//
// & przed lat_out/lon_out = referencja — funkcja MODYFIKUJE oryginał,
// nie kopię. To ważna różnica od przekazywania przez wartość!
void trueAnomalyToLatLon(double true_anomaly_deg,
                         double inclination_deg,
                         double& lat_out,
                         double& lon_out) {
    double anom_rad  = true_anomaly_deg * DEG_TO_RAD;
    double incl_rad  = inclination_deg  * DEG_TO_RAD;

    // sin(incl) * sin(anom) daje składową N-S ruchu
    lat_out = std::asin(std::sin(incl_rad) * std::sin(anom_rad)) * RAD_TO_DEG;

    // atan2 daje kąt w pełnym zakresie [-180, 180]
    // cos(incl) * sin(anom) to składowa E-W skorygowana inklinacją
    lon_out = std::atan2(std::cos(anom_rad),
                         std::cos(incl_rad) * std::sin(anom_rad)) * RAD_TO_DEG;

    // fmod = modulo dla double — utrzymuje lon w [-180, 180]
    lon_out = std::fmod(lon_out + 180.0, 360.0) - 180.0;
}

// ── Główny task orbity ────────────────────────────────────
void vOrbitTask(void* pvParameters) {
    // ── CAST z void* na nasz typ — klasyczny FreeRTOS pattern ──
    // FreeRTOS nie zna C++ typów, przekazuje surowy wskaźnik void*.
    // static_cast<T*> to bezpieczna wersja rzutowania — kompilator
    // sprawdza czy konwersja ma sens. Nigdy nie używaj (T*) w C++!
    OrbitTaskParams* params = static_cast<OrbitTaskParams*>(pvParameters);
    QueueHandle_t queue     = params->output_queue;

    double true_anomaly = 0.0;   // startujemy z pozycji 0° na orbicie

    // Okres orbitalny T = 2π * sqrt(r³/μ)
    // Dla 550km ≈ 5730 sekund ≈ 95 minut
    // Co sekundę anomalia rośnie o 360° / T sekund
    const double alt  = g_sim_conditions.orbit_altitude_km;
    const double incl = g_sim_conditions.orbit_inclination;
    const double r    = (EARTH_RADIUS_KM + alt) * 1000.0;
    const double T_s  = 2.0 * PI * std::sqrt((r * r * r) / MU_EARTH);
    const double angle_per_second = 360.0 / T_s;

    std::cout << "[ORBIT] Start. T=" << (int)T_s << "s, "
              << "v=" << (int)calculateOrbitalVelocity(alt) << "m/s\n";

    // ── NIESKOŃCZONA PĘTLA — serce każdego taska FreeRTOS ──
    // Task nigdy nie może "wyjść" z pętli (return) bo FreeRTOS
    // nie wie co z nim zrobić. Jeśli task kończy działanie,
    // musi wywołać vTaskDelete(NULL) na sobie samym.
    for (;;) {
        // Odczytaj aktualne warunki (mogły zmienić się z SimControl)
        // volatile gdyby kompilator nie chciał czytać za każdym razem
        double current_alt  = g_sim_conditions.orbit_altitude_km;
        double current_incl = g_sim_conditions.orbit_inclination;

        double lat, lon;
        trueAnomalyToLatLon(true_anomaly, current_incl, lat, lon);

        OrbitData data;
        data.latitude     = lat;
        data.longitude    = lon;
        data.altitude_km  = current_alt;
        data.velocity_ms  = calculateOrbitalVelocity(current_alt);
        data.inclination  = current_incl;
        data.true_anomaly = true_anomaly;
        data.timestamp_ms = xTaskGetTickCount(); // FreeRTOS tick jako czas

        // xQueueSend — wyślij do kolejki
        // pdMS_TO_TICKS(10) = czekaj max 10ms jeśli kolejka pełna
        // pdTRUE = 0 = nie wysyłaj na początek kolejki
        if (xQueueSend(queue, &data, pdMS_TO_TICKS(10)) != pdPASS) {
            std::cerr << "[ORBIT] WARN: kolejka pełna!\n";
        }

        // Przesuń satelitę na orbicie
        // angle_per_second * (ORBIT_TASK_PERIOD_MS/1000.0) = kąt na tick
        true_anomaly += angle_per_second * (ORBIT_TASK_PERIOD_MS / 1000.0);
        if (true_anomaly >= 360.0) true_anomaly -= 360.0; // wrap 360→0

        // vTaskDelay — oddaj CPU innym taskom na N ticków
        // pdMS_TO_TICKS konwertuje ms → ticki schedulera
        // BEZ vTaskDelay task zjadałby 100% CPU (busy-wait)!
        vTaskDelay(pdMS_TO_TICKS(ORBIT_TASK_PERIOD_MS));
    }
}

// ── Task łącza laserowego ─────────────────────────────────
void vLaserTask(void* pvParameters) {
    QueueHandle_t queue = static_cast<QueueHandle_t>(pvParameters);

    // Seed dla generatora losowego — w embedded używa się hardware RNG
    // Tu symulujemy szum sygnału jako małe losowe odchylenia
    uint32_t noise_counter = 0;

    for (;;) {
        const SimConditions& cond = g_sim_conditions; // referencja = bez kopii

        LaserLinkData laser;
        laser.timestamp_ms = xTaskGetTickCount();

        // Łącze nieaktywne gdy: zaćmienie LUB silne zachmurzenie
        laser.link_active = !cond.eclipse && (cond.cloud_coverage < 0.85f);

        if (laser.link_active) {
            // Bazowa moc sygnału [dBm]
            // -30 dBm to dobry sygnał, -60 to granica użyteczności
            float base_signal = -30.0f;

            // Tłumienie od chmur: chmury pochłaniają i rozpraszają laser
            // Każde 10% zachmurzenia = ~3 dB straty (przybliżenie)
            float cloud_loss = cond.cloud_coverage * 30.0f;

            // Scyntylacja = losowe fluktuacje od turbulencji atmosferycznych
            // symulujemy prostym szumem: counter daje zmienną wartość
            noise_counter++;
            float scint_noise = cond.atmospheric_scint * 5.0f *
                                std::sin(noise_counter * 0.7f); // pseudo-szum

            laser.atmospheric_loss  = cloud_loss;
            laser.signal_strength   = base_signal - cloud_loss + scint_noise;

            // BER (Bit Error Rate) rośnie eksponencjalnie ze słabością sygnału
            // Wzór uproszczony: BER ≈ 0.5 * erfc(SNR/sqrt(2))
            // My używamy prostszej aproksymacji liniowej:
            float snr_factor = (laser.signal_strength + 60.0f) / 30.0f;
            snr_factor = std::max(0.0f, std::min(1.0f, snr_factor)); // clamp [0,1]
            laser.bit_error_rate = std::pow(10.0f, -9.0f + (1.0f - snr_factor) * 9.0f);

            // Ping: bazowo 240ms (GEO) ale LEO to ok 5-20ms do stacji
            // Dodajemy opóźnienie od zachmurzenia i turbulencji
            laser.ping_ms = 8 + (uint32_t)(cond.cloud_coverage * 40.0f)
                              + (uint32_t)(cond.atmospheric_scint * 20.0f);
        } else {
            // Brak łącza — zerujemy wartości
            laser.signal_strength  = -99.0f;
            laser.bit_error_rate   = 1.0f;
            laser.atmospheric_loss = 99.0f;
            laser.ping_ms          = 0;
        }

        if (xQueueSend(queue, &laser, pdMS_TO_TICKS(10)) != pdPASS) {
            std::cerr << "[LASER] WARN: kolejka pełna!\n";
        }

        vTaskDelay(pdMS_TO_TICKS(LASER_TASK_PERIOD_MS));
    }
}

} // namespace Satellite