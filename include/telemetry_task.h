#ifndef TELEMETRY_TASK_H
#define TELEMETRY_TASK_H

#include "FreeRTOS.h"
#include "queue.h"
#include "common_types.h"

namespace Satellite {

    // ── Parametry dla TelemetryTask ───────────────────────
    // Ten sam pattern co OrbitTaskParams — FreeRTOS daje nam
    // tylko jeden void*, więc pakujemy wszystko w strukturę.
    // To standardowy idiom w każdym projekcie FreeRTOS!
    struct TelemetryTaskParams {
        QueueHandle_t sensor_queue;   // dane IMU z SensorTask
        QueueHandle_t orbit_queue;    // dane orbitalne z OrbitTask
        QueueHandle_t laser_queue;    // dane lasera z LaserTask
    };

    void vTelemetryTask(void* pvParameters);

} // namespace Satellite

#endif