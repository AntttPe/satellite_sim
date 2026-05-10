#ifndef ORBIT_TASK_H
#define ORBIT_TASK_H

#include "FreeRTOS.h"
#include "queue.h"
#include "common_types.h"

namespace Satellite {

    // ── OrbitTask params — teraz z dwoma kolejkami ────────
    struct OrbitTaskParams {
        QueueHandle_t output_queue;  // → TelemetryTask
        QueueHandle_t socket_queue;  // → SocketSender (nowe!)
        SimConditions* conditions;   // wskaźnik na warunki sim
    };

    // ── LaserTask params — fan-out jak SensorTask ─────────
    // LaserTask był wcześniej bez struktury (dostawał jedną kolejkę).
    // Teraz musi pisać do dwóch — potrzebuje struktury.
    struct LaserTaskParams {
        QueueHandle_t telem_queue;   // → TelemetryTask
        QueueHandle_t socket_queue;  // → SocketSender
    };

    // extern = "ta zmienna istnieje w orbit_task.cpp"
    // Bez extern każdy plik który include'uje ten header
    // tworzyłby własną kopię — błąd linkera "multiple definition"
    extern SimConditions g_sim_conditions;

    double calculateOrbitalVelocity(double altitude_km);

    void trueAnomalyToLatLon(double true_anomaly_deg,
                             double inclination_deg,
                             double& lat_out,
                             double& lon_out);

    void vOrbitTask(void* pvParameters);
    void vLaserTask(void* pvParameters);

} // namespace Satellite

#endif