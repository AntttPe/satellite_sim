#include "sensor_task.h"
#include "watchdog_task.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <cmath>
#include <random>
#include <iostream>

namespace Satellite {

static std::mt19937 rng_engine(42);
static std::normal_distribution<float> noise(0.0f, 1.0f);

static float simulateGyroX(float time_s) {
    return 0.5f * std::sin(time_s / 5400.0f * 2.0f * M_PI)
           + noise(rng_engine) * 0.05f;
}

static float simulateTemperature(float time_s) {
    float base = 20.0f + 40.0f * std::sin(time_s / 5400.0f * 2.0f * M_PI);
    return base + noise(rng_engine) * 0.5f;
}

SensorData generateSensorReading(uint32_t timestamp_ms) {
    float t = static_cast<float>(timestamp_ms) / 1000.0f;

    SensorData data;
    data.timestamp_ms = timestamp_ms;
    data.gyro_x       = simulateGyroX(t);
    data.gyro_y       = 0.3f * std::cos(t / 5400.0f) + noise(rng_engine) * 0.05f;
    data.gyro_z       = 0.1f + noise(rng_engine) * 0.02f;
    data.accel_x      = noise(rng_engine) * 0.01f;
    data.accel_y      = noise(rng_engine) * 0.01f;
    data.accel_z      = 9.81f + noise(rng_engine) * 0.01f;
    data.temperature  = simulateTemperature(t);

    return data;
}

void vSensorTask(void* pvParameters) {

    // ── static_cast zamiast reinterpret_cast ──────────────
    // Poprzednia wersja używała reinterpret_cast<QueueHandle_t>
    // co było błędem — teraz przekazujemy strukturę SensorTaskParams.
    // static_cast<SensorTaskParams*> jest bezpieczniejszy:
    // kompilator sprawdza czy void* → SensorTaskParams* ma sens.
    // reinterpret_cast omija te sprawdzenia — używaj tylko dla
    // konwersji hardware registers lub typów zupełnie niezwiązanych.
    SensorTaskParams* params = static_cast<SensorTaskParams*>(pvParameters);

    // ── Wyciągnij kolejki ze struktury ───────────────────
    // Lokalne zmienne dla czytelności — zamiast params->telem_queue wszędzie
    QueueHandle_t telem_queue  = params->telem_queue;
    QueueHandle_t socket_queue = params->socket_queue;

    std::cout << "[SENSOR] Task wystartował" << std::endl;

    TickType_t last_wake_time = xTaskGetTickCount();

    while (true) {

        // xTaskDelayUntil — dokładniejszy timing niż vTaskDelay
        // vTaskDelay(100ms):      "czekaj 100ms OD TEJ CHWILI"
        //                          jeśli generowanie danych zajęło 5ms
        //                          → rzeczywisty okres = 105ms (dryfuje!)
        // xTaskDelayUntil(100ms): "czekaj AŻ MINIE 100ms od last_wake_time"
        //                          czas generowania jest odjęty automatycznie
        //                          → rzeczywisty okres = dokładnie 100ms
        // W systemach real-time to krytyczna różnica!
        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS));

        SensorData data = generateSensorReading(xTaskGetTickCount());

        // ── FAN-OUT: wyślij do DWÓCH kolejek ─────────────
        // To serce nowej architektury — ten sam pakiet
        // trafia do TelemetryTask (konsola) i SocketSender (Python).
        //
        // Kolejność ma znaczenie: najpierw telem, potem socket.
        // Gdyby telem_queue była pełna i czekaliśmy 10ms,
        // socket też dostaje dane z 10ms opóźnieniem.
        // W praktyce kolejki nie powinny być pełne przy
        // normalnym działaniu systemu.

        // → TelemetryTask (wyświetlanie w konsoli)
        if (xQueueSend(telem_queue, &data, pdMS_TO_TICKS(10)) != pdTRUE) {
            std::cout << "[SENSOR] WARN: telem_queue pełna!" << std::endl;
        }

        // → SocketSender (JSON do Python FastAPI)
        // Timeout 0 zamiast 10ms — socket może być wolniejszy
        // (zależy od sieci), nie chcemy blokować SensorTask.
        // Lepiej odrzucić pakiet niż opóźnić kolejny pomiar IMU.
        if (xQueueSend(socket_queue, &data, 0) != pdTRUE) {
            // Nie drukujemy WARN bo socket może być niepołączony
            // na początku działania — to normalny stan
        }

        // ── Heartbeat dla Watchdoga ───────────────────────
        // xSemaphoreGive = "daj" semafor (stan 0→1)
        // WatchdogTask robi Take() z timeoutem 5000ms
        // Brak Give() przez 5s = SensorTask zawiesił się = ALARM
        xSemaphoreGive(Satellite::g_watchdog_semaphore);
    }
}

} // namespace Satellite