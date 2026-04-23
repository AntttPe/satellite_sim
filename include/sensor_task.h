#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "FreeRTOS.h"
#include "queue.h"               // QueueHandle_t
#include "common_types.h"

namespace Satellite {

    // Deklaracja funkcji taska — implementacja będzie w sensor_task.cpp
    // void* pvParameters — klasyczny sposób przekazywania parametrów do tasków FreeRTOS
    // (FreeRTOS jest w C, nie zna szablonów C++)
    void vSensorTask(void* pvParameters);

    // Funkcja pomocnicza — generuje realistyczne dane z szumem
    // Poznasz tu: parametry by reference, losowość w C++
    SensorData generateSensorReading(uint32_t timestamp_ms);

} // namespace Satellite

#endif // SENSOR_TASK_H