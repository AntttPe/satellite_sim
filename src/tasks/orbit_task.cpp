#include "orbit_task.h"
#include "task.h"
#include "queue.h"
#include <cmath>
#include <iostream>

namespace Satellite {

SimConditions g_sim_conditions = {
    .cloud_coverage    = 0.2f,
    .atmospheric_scint = 0.1f,
    .solar_irradiance  = 1361.0f,
    .eclipse           = false,
    .orbit_altitude_km = 550.0f,
    .orbit_inclination = 53.0f
};

double calculateOrbitalVelocity(double altitude_km) {
    double r = (EARTH_RADIUS_KM + altitude_km) * 1000.0;
    return std::sqrt(MU_EARTH / r);
}

void trueAnomalyToLatLon(double true_anomaly_deg,
                         double inclination_deg,
                         double& lat_out,
                         double& lon_out) {
    double anom_rad = true_anomaly_deg * DEG_TO_RAD;
    double incl_rad = inclination_deg  * DEG_TO_RAD;

    lat_out = std::asin(std::sin(incl_rad) * std::sin(anom_rad)) * RAD_TO_DEG;
    lon_out = std::atan2(std::cos(anom_rad),
                         std::cos(incl_rad) * std::sin(anom_rad)) * RAD_TO_DEG;
    lon_out = std::fmod(lon_out + 180.0, 360.0) - 180.0;
}

// ════════════════════════════════════════════════════════
// vOrbitTask
// ════════════════════════════════════════════════════════
void vOrbitTask(void* pvParameters) {
    OrbitTaskParams* params = static_cast<OrbitTaskParams*>(pvParameters);

    // ── DLACZEGO wyciągamy do lokalnych zmiennych? ────────
    // params->output_queue za każdym razem = dereferencja wskaźnika
    // przy każdym użyciu. Lokalny QueueHandle_t = jedna dereferencja
    // przy starcie, potem bezpośredni dostęp. Szybsze i czytelniejsze.
    // To micro-optymalizacja ale dobry nawyk w embedded.
    QueueHandle_t telem_q  = params->output_queue;
    QueueHandle_t socket_q = params->socket_queue;

    double true_anomaly = 0.0;

    const double alt  = g_sim_conditions.orbit_altitude_km;
    const double incl = g_sim_conditions.orbit_inclination;
    const double r    = (EARTH_RADIUS_KM + alt) * 1000.0;
    const double T_s  = 2.0 * PI * std::sqrt((r * r * r) / MU_EARTH);
    const double angle_per_second = 360.0 / T_s;

    std::cout << "[ORBIT] Start. T=" << (int)T_s << "s, "
              << "v=" << (int)calculateOrbitalVelocity(alt) << "m/s\n";

    for (;;) {
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
        data.timestamp_ms = xTaskGetTickCount();

        // ── FAN-OUT do dwóch kolejek ──────────────────────
        // POPRZEDNI KOD: xQueueSend(queue, ...) — błąd!
        // Zmienna "queue" nie istniała — były telem_q i socket_q
        // ale kod używał niezadeklarowanej "queue".
        // To był błąd kompilacji który musiał być ukryty w starszej
        // wersji gdzie params miało tylko output_queue bez socket_queue.
        //
        // NOWY KOD: jawny fan-out do obu kolejek.
        // telem_q  → TelemetryTask czyta i wyświetla w konsoli
        // socket_q → SocketSender czyta i wysyła JSON do Pythona
        if (xQueueSend(telem_q, &data, pdMS_TO_TICKS(10)) != pdPASS) {
            std::cerr << "[ORBIT] WARN: telem_q pełna!\n";
        }

        // Timeout 0 dla socket_q — nie blokujemy OrbitTask
        // jeśli SocketSender jest zajęty lub niepołączony.
        // Utrata jednego pakietu orbity to ok — następny
        // przyjdzie za sekundę z aktualną pozycją.
        if (xQueueSend(socket_q, &data, 0) != pdPASS) {
            // brak warnu — normalny stan gdy Python nie połączony
        }

        true_anomaly += angle_per_second * (ORBIT_TASK_PERIOD_MS / 1000.0);
        if (true_anomaly >= 360.0) true_anomaly -= 360.0;

        vTaskDelay(pdMS_TO_TICKS(ORBIT_TASK_PERIOD_MS));
    }
}

// ════════════════════════════════════════════════════════
// vLaserTask
// ════════════════════════════════════════════════════════
void vLaserTask(void* pvParameters) {

    // ── DLACZEGO zmiana z QueueHandle_t na LaserTaskParams? ──
    // Stary kod: QueueHandle_t queue = static_cast<QueueHandle_t>(pvParameters)
    // To było NIEBEZPIECZNE — QueueHandle_t to wskaźnik (void* pod spodem),
    // więc static_cast<QueueHandle_t>(void*) kompiluje się ale jest
    // semantycznie błędne: rzutujesz void* na void* i traktujesz
    // jedną kolejkę jakby była całym zestawem parametrów.
    //
    // Nowy kod: static_cast<LaserTaskParams*> — bezpieczne i jawne.
    // Kompilator wie że pvParameters to wskaźnik na LaserTaskParams,
    // dostęp przez -> jest type-safe.
    LaserTaskParams* params = static_cast<LaserTaskParams*>(pvParameters);
    QueueHandle_t telem_q   = params->telem_queue;
    QueueHandle_t socket_q  = params->socket_queue;

    uint32_t noise_counter = 0;

    for (;;) {
        // const& = referencja tylko do odczytu — dwie zalety:
        // 1. nie kopiujemy całej struktury SimConditions (oszczędność pamięci)
        // 2. const gwarantuje że przypadkowo nie zmodyfikujemy g_sim_conditions
        const SimConditions& cond = g_sim_conditions;

        LaserLinkData laser;
        laser.timestamp_ms = xTaskGetTickCount();

        laser.link_active = !cond.eclipse && (cond.cloud_coverage < 0.85f);

        if (laser.link_active) {
            float base_signal = -30.0f;
            float cloud_loss  = cond.cloud_coverage * 30.0f;

            noise_counter++;
            float scint_noise = cond.atmospheric_scint * 5.0f *
                                std::sin(noise_counter * 0.7f);

            laser.atmospheric_loss = cloud_loss;
            laser.signal_strength  = base_signal - cloud_loss + scint_noise;

            float snr_factor = (laser.signal_strength + 60.0f) / 30.0f;

            // std::max / std::min = clamp wartości do zakresu [0, 1]
            // bez tego snr_factor mógłby wyjść poza zakres i pow()
            // dałby nonsensowne wyniki (ujemne BER lub >1.0)
            snr_factor = std::max(0.0f, std::min(1.0f, snr_factor));

            laser.bit_error_rate = std::pow(
                10.0f,
                -9.0f + (1.0f - snr_factor) * 9.0f
            );

            laser.ping_ms = 8
                + static_cast<uint32_t>(cond.cloud_coverage    * 40.0f)
                + static_cast<uint32_t>(cond.atmospheric_scint * 20.0f);
                // static_cast<uint32_t> zamiast (uint32_t) — jawna
                // konwersja float→int, kompilator nie zgłosi warningów
        } else {
            laser.signal_strength  = -99.0f;
            laser.bit_error_rate   = 1.0f;
            laser.atmospheric_loss = 99.0f;
            laser.ping_ms          = 0;
        }

        // ── FAN-OUT identyczny jak w OrbitTask ───────────
        if (xQueueSend(telem_q, &laser, pdMS_TO_TICKS(10)) != pdPASS) {
            std::cerr << "[LASER] WARN: telem_q pełna!\n";
        }
        if (xQueueSend(socket_q, &laser, 0) != pdPASS) {
            // brak warnu — normalny stan przy braku połączenia
        }

        vTaskDelay(pdMS_TO_TICKS(LASER_TASK_PERIOD_MS));
    }
}

} // namespace Satellite