#include "sensor_task.h"         // nasza deklaracja funkcji i typów
#include "watchdog_task.h"       // potrzebujemy g_watchdog_semaphore żeby kickować watchdoga
#include "FreeRTOS.h"            // typy bazowe kernela
#include "task.h"                // xTaskDelayUntil, xTaskGetTickCount
#include "queue.h"               // xQueueSend, QueueHandle_t

#include <cmath>                 // std::sin, std::cos, M_PI — funkcje matematyczne
#include <random>                // nowoczesny generator liczb losowych C++17
#include <iostream>              // std::cout

namespace Satellite {

// static = zmienna istnieje przez cały czas działania programu
//          ale widoczna tylko w tym pliku (nie wycieka do innych .cpp)
// mt19937 = Mersenne Twister — wysokiej jakości generator pseudolosowy
// (42) = seed — ten sam seed daje te same "losowe" liczby przy każdym uruchomieniu
//        przydatne do reprodukowalnych testów
static std::mt19937 rng_engine(42);

// rozkład normalny: średnia=0.0, odchylenie standardowe=1.0
// modeluje szum czujnika — większość odczytów blisko 0,
// sporadycznie dalej (tak działa prawdziwy IMU)
static std::normal_distribution<float> noise(0.0f, 1.0f);

// funkcja pomocnicza — symuluje odczyt żyroskopu wokół osi X
// time_s = czas w sekundach od startu symulacji
static float simulateGyroX(float time_s) {
    // satelita na orbicie LEO (Low Earth Orbit) obraca się powoli
    // amplituda 0.5 deg/s, okres 5400s = 90 minut (jedna orbita)
    // std::sin zwraca wartość z zakresu [-1, 1]
    // noise(rng_engine) * 0.05f = szum czujnika, 5% amplitudy
    return 0.5f * std::sin(time_s / 5400.0f * 2.0f * M_PI)
           + noise(rng_engine) * 0.05f;
}

// symuluje temperaturę panelu słonecznego
// zmienia się między -20°C (cień Ziemi) a +60°C (bezpośrednie słońce)
static float simulateTemperature(float time_s) {
    float base = 20.0f + 40.0f * std::sin(time_s / 5400.0f * 2.0f * M_PI);
    return base + noise(rng_engine) * 0.5f;    // + szum 0.5°C
}

// publiczna funkcja — buduje kompletny obiekt SensorData
SensorData generateSensorReading(uint32_t timestamp_ms) {

    // static_cast<float> = jawna konwersja uint32_t → float
    // bez tego: uint32_t / 1000 = dzielenie całkowite (obcina resztę!)
    // nowoczesny C++ preferuje static_cast zamiast rzutowania w stylu C: (float)timestamp_ms
    float t = static_cast<float>(timestamp_ms) / 1000.0f;

    SensorData data;                   // tworzymy obiekt na stosie (stack) — szybkie, automatyczne
    data.timestamp_ms = timestamp_ms;
    data.gyro_x       = simulateGyroX(t);
    data.gyro_y       = 0.3f * std::cos(t / 5400.0f) + noise(rng_engine) * 0.05f;
    data.gyro_z       = 0.1f + noise(rng_engine) * 0.02f;
    data.accel_x      = noise(rng_engine) * 0.01f;       // mikrograwit. ≈ 0 g
    data.accel_y      = noise(rng_engine) * 0.01f;
    data.accel_z      = 9.81f + noise(rng_engine) * 0.01f;  // ≈ 1g w osi Z
    data.temperature  = simulateTemperature(t);

    return data;    // Return Value Optimization (RVO) — kompilator nie kopiuje struktury,
                    // buduje ją bezpośrednio w miejscu docelowym — zero kosztu kopiowania
}

void vSensorTask(void* pvParameters) {

    // reinterpret_cast = "reinterpretuj te same bity jako inny typ"
    // pvParameters to void* — nie ma informacji o typie
    // wiemy że przekazaliśmy QueueHandle_t w main(), więc rzutujemy z powrotem
    // używaj reinterpret_cast tylko gdy jesteś pewien co tam jest
    QueueHandle_t telemetry_queue = reinterpret_cast<QueueHandle_t>(pvParameters);

    std::cout << "[SENSOR] Task wystartował" << std::endl;

    // TickType_t = typ FreeRTOS dla ticków schedulera (uint32_t pod spodem)
    // zapamiętujemy moment startu — potrzebne dla xTaskDelayUntil
    TickType_t last_wake_time = xTaskGetTickCount();

    while (true) {

        // xTaskDelayUntil jest dokładniejsze niż vTaskDelay
        // vTaskDelay(100):       "czekaj 100ms od teraz"
        // xTaskDelayUntil(100):  "czekaj aż minie 100ms od last_wake_time"
        // różnica: xTaskDelayUntil kompensuje czas wykonania kodu w pętli
        // dzięki temu pomiary są dokładnie co SENSOR_TASK_PERIOD_MS (100ms)
        // nie "100ms + czas obliczeń"
        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS));

        // generuj dane — xTaskGetTickCount() zwraca aktualny tick schedulera
        SensorData data = generateSensorReading(xTaskGetTickCount());

        // wyślij do kolejki — xQueueSend(kolejka, wskaźnik_na_dane, timeout)
        // &data = adres naszej lokalnej zmiennej — kernel skopiuje te bajty do kolejki
        // pdMS_TO_TICKS(10) = czekaj max 10ms jeśli kolejka pełna
        BaseType_t sent = xQueueSend(telemetry_queue, &data, pdMS_TO_TICKS(10));

        if (sent != pdTRUE) {
            // kolejka pełna — TelemetryTask nie nadąża z odbiorem
            std::cout << "[SENSOR] WARN: kolejka pełna, pakiet odrzucony!" << std::endl;
        }

        // sygnalizuj watchdogowi że SensorTask żyje i działa
        // xSemaphoreGive = "daj" semafor — ustawia stan semafora na 1
        // WatchdogTask czeka na ten sygnał z timeoutem WATCHDOG_TIMEOUT_MS
        // jeśli SensorTask się zawiesi — Give nigdy nie zostanie wywołane
        // → WatchdogTask dostanie timeout → alarm
        xSemaphoreGive(Satellite::g_watchdog_semaphore);

    }   // koniec while(true)
}       // koniec vSensorTask

} // namespace Satellite