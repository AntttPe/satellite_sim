#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "FreeRTOS.h"
#include "queue.h"
#include "common_types.h"

namespace Satellite {

    // ── Nowa struktura parametrów ─────────────────────────
    // SensorTask musi pisać do DWÓCH kolejek jednocześnie:
    //   telem_queue  → TelemetryTask (wyświetlanie w konsoli)
    //   socket_queue → SocketSender  (wysyłanie JSON do Python)
    //
    // To pattern "fan-out" — jeden producent, wielu odbiorców.
    // Alternatywa to event bus ale w FreeRTOS queue fan-out
    // jest prostszy i bardziej przewidywalny pamięciowo.
    struct SensorTaskParams {
        QueueHandle_t telem_queue;   // → TelemetryTask
        QueueHandle_t socket_queue;  // → SocketSender
    };

    void vSensorTask(void* pvParameters);
    SensorData generateSensorReading(uint32_t timestamp_ms);

} // namespace Satellite

#endif