#include <iostream>              // std::cout, std::cerr
#include "FreeRTOS.h"            // typy bazowe kernela FreeRTOS
#include "task.h"                // xTaskCreate, vTaskStartScheduler
#include "queue.h"               // xQueueCreate, QueueHandle_t
#include "semphr.h"              // xSemaphoreCreateBinary, SemaphoreHandle_t
#include "sensor_task.h"         // Satellite::vSensorTask
#include "telemetry_task.h"      // Satellite::vTelemetryTask
#include "watchdog_task.h"       // Satellite::vWatchdogTask, g_watchdog_semaphore
#include "common_types.h"        // Satellite::SensorData i stałe

int main() {

    std::cout << "=== Satellite Sim Boot ===" << std::endl;

    // xQueueCreate(liczba_miejsc, rozmiar_jednego_elementu)
    // kernel alokuje pamięć z puli configTOTAL_HEAP_SIZE (64KB w naszym config)
    // zwraca nullptr jeśli brak pamięci
    QueueHandle_t telemetry_queue = xQueueCreate(10, sizeof(Satellite::SensorData));

    if (telemetry_queue == nullptr) {       // nullptr = pusty wskaźnik, nowoczesny C++
        std::cerr << "BŁĄD: brak pamięci na kolejkę!" << std::endl;
        return 1;                           // niezerowy kod wyjścia = błąd
    }

    // semafor binarny — jak przełącznik, ma stan 0 lub 1
    // xSemaphoreCreateBinary tworzy go w stanie 0 (nie sygnalizowany)
    // SensorTask będzie robić Give (→1), WatchdogTask będzie robić Take (→0)
    Satellite::g_watchdog_semaphore = xSemaphoreCreateBinary();

    if (Satellite::g_watchdog_semaphore == nullptr) {
        std::cerr << "BŁĄD: brak pamięci na semafor!" << std::endl;
        return 1;
    }

    // xTaskCreate(funkcja, nazwa, rozmiar_stosu, parametry, priorytet, uchwyt)
    // priorytet 3 = najwyższy — watchdog musi zawsze działać, nigdy nie może
    //              być zagłodzony przez inne taski
    // priorytet 2 = SensorTask — zbieranie danych jest krytyczne czasowo
    // priorytet 1 = TelemetryTask — przetwarzanie może chwilę poczekać
    xTaskCreate(Satellite::vSensorTask,    "SensorTask", 512, telemetry_queue, 2, nullptr);
    xTaskCreate(Satellite::vTelemetryTask, "TelemTask",  512, telemetry_queue, 1, nullptr);
    xTaskCreate(Satellite::vWatchdogTask,  "Watchdog",   256, nullptr,         3, nullptr);

    std::cout << "[BOOT] Start schedulera..." << std::endl;

    // od tej linii FreeRTOS przejmuje kontrolę na zawsze
    // main() przestaje istnieć jako "program główny" — scheduler zarządza CPU
    // ta funkcja nigdy nie wraca jeśli wszystko działa poprawnie
    vTaskStartScheduler();

    // tutaj dochodzimy tylko jeśli scheduler się zatrzymał
    // w praktyce oznacza to poważny błąd (brak pamięci na idle task)
    std::cerr << "BŁĄD: Scheduler zatrzymany" << std::endl;
    return 1;
}