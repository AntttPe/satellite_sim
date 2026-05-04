#ifndef ORBIT_TASK_H
#define ORBIT_TASK_H

#include "FreeRTOS.h"
#include "queue.h"
#include "common_types.h"

namespace Satellite {

    struct OrbitTaskParams {
        QueueHandle_t output_queue;   // tu wysyłamy OrbitData
        SimConditions* conditions;    // wskaźnik na warunki sim (shared)
    };

    extern SimConditions g_sim_conditions;

    // Oblicza prędkość orbitalną z prawa Keplera: v = sqrt(mu / r)
    // Zwraca [m/s]. Im niżej orbita, tym szybciej satelita leci.
    double calculateOrbitalVelocity(double altitude_km);

    void trueAnomalyToLatLon(double true_anomaly_deg,
                             double inclination_deg,
                             double& lat_out,
                             double& lon_out);

    void vOrbitTask(void* pvParameters);
    void vLaserTask(void* pvParameters);  // osobny task dla łącza

} // namespace Satellite

#endif