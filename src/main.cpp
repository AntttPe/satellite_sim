#include <iostream>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "sensor_task.h"
#include "telemetry_task.h"
#include "watchdog_task.h"
#include "common_types.h"
#include "orbit_task.h"

int main() {
    std::cout << "=== Satellite Sim Boot ===" << std::endl;

    // ── KOLEJKA TELEMETRII (stara, niezmieniona) ──────────
    // SensorTask → [telemetry_queue] → TelemetryTask
    // sizeof(SensorData) = rozmiar jednego "slotu" w kolejce
    // FreeRTOS kopiuje całą strukturę do kolejki (nie wskaźnik!)
    // To ważne: struktura musi być mała, duże dane = wolno
    QueueHandle_t telemetry_queue = xQueueCreate(10, sizeof(Satellite::SensorData));
    if (telemetry_queue == nullptr) {
        std::cerr << "BŁĄD: brak pamięci na kolejkę telemetrii!" << std::endl;
        return 1;
    }

    // ── KOLEJKA DANYCH ORBITALNYCH (nowa) ─────────────────
    // OrbitTask → [orbit_queue] → TelemetryTask (do wysłania dalej)
    // 5 miejsc wystarczy — orbit update co 1s, telemetry czyta co 1s
    QueueHandle_t orbit_queue = xQueueCreate(5, sizeof(Satellite::OrbitData));
    if (orbit_queue == nullptr) {
        std::cerr << "BŁĄD: brak pamięci na kolejkę orbity!" << std::endl;
        return 1;
    }

    // ── KOLEJKA DANYCH LASEROWYCH (nowa) ──────────────────
    // LaserTask → [laser_queue] → TelemetryTask
    // 10 miejsc bo laser update co 500ms, może się nagromadzić
    QueueHandle_t laser_queue = xQueueCreate(10, sizeof(Satellite::LaserLinkData));
    if (laser_queue == nullptr) {
        std::cerr << "BŁĄD: brak pamięci na kolejkę lasera!" << std::endl;
        return 1;
    }

    // ── SEMAFOR WATCHDOGA (niezmieniony) ──────────────────
    // SensorTask co chwilę robi Give() na tym semaforze
    // WatchdogTask sprawdza czy Give() przyszło w ciągu 5s
    // Brak Give() = SensorTask padł = system w błędzie
    Satellite::g_watchdog_semaphore = xSemaphoreCreateBinary();
    if (Satellite::g_watchdog_semaphore == nullptr) {
        std::cerr << "BŁĄD: brak pamięci na semafor!" << std::endl;
        return 1;
    }

    // ── PARAMETRY DLA ORBIT TASK ───────────────────────────
    // DLACZEGO static? Wyjaśnienie krok po kroku:
    //
    // Zwykła zmienna lokalna żyje na stosie funkcji main().
    // Stos main() formalnie istnieje do końca funkcji.
    // Ale vTaskStartScheduler() nigdy nie wraca — więc stos
    // main() "wisi" w pamięci, ale kompilator może go w teorii
    // zoptymalizować lub zaalokować nad nim coś innego.
    //
    // static local = zmienna trafia do sekcji .data (pamięć statyczna),
    // nie na stos. Żyje przez cały czas działania programu.
    // Bezpieczne do przekazywania wskaźnika do taska!
    //
    // Alternatywa: xTaskCreate z new OrbitTaskParams{...}
    // ale wtedy musisz pamiętać o delete (a w FreeRTOS tasków
    // zazwyczaj się nie usuwa) — static jest prostsze.
    static Satellite::OrbitTaskParams orbit_params = {
        .output_queue = orbit_queue,
        .conditions   = &Satellite::g_sim_conditions  // wskaźnik na global
    };

    // ── TWORZENIE TASKÓW ───────────────────────────────────
    // Kolejność tworzenia NIE MA znaczenia dla kolejności wykonania!
    // O kolejności decyduje scheduler na podstawie PRIORYTETU.
    // Scheduler startuje dopiero po vTaskStartScheduler() poniżej.
    //
    // Priorytety w naszym systemie:
    //   3 = Watchdog    — musi zawsze działać, nikt go nie zagłodzi
    //   2 = SensorTask  — dane IMU krytyczne czasowo
    //   2 = OrbitTask   — fizyka orbity, update co 1s
    //   2 = LaserTask   — symulacja łącza, update co 0.5s
    //   1 = TelemTask   — przetwarzanie może chwilę poczekać

    xTaskCreate(Satellite::vSensorTask,
                "SensorTask",
                512,            // rozmiar stosu taska w słowach (nie bajtach!)
                telemetry_queue,// parametr: kolejka do której piszemy
                2,              // priorytet
                nullptr);       // uchwyt taska — nullptr jeśli nie potrzebujemy

    static Satellite::TelemetryTaskParams telem_params = {
        .sensor_queue = telemetry_queue,
        .orbit_queue  = orbit_queue,
        .laser_queue  = laser_queue
    };

    xTaskCreate(Satellite::vTelemetryTask, "TelemTask", 1024,
                &telem_params, 1, nullptr);

    xTaskCreate(Satellite::vWatchdogTask,
                "Watchdog",
                256,            // watchdog ma mały stos — robi mało rzeczy
                nullptr,        // nie potrzebuje parametrów (używa globalnego semafora)
                3,
                nullptr);

    // ── NOWE TASKI ─────────────────────────────────────────

    // OrbitTask dostaje WSKAŹNIK na orbit_params (stąd &)
    // Wewnątrz taska: static_cast<OrbitTaskParams*>(pvParameters)
    xTaskCreate(Satellite::vOrbitTask,
                "OrbitTask",
                1024,           // większy stos — używamy cmath (sin/cos/sqrt)
                                // funkcje trygonometryczne potrzebują miejsca na stosie
                &orbit_params,  // wskaźnik na strukturę z kolejką i warunkami
                2,
                nullptr);

    // LaserTask dostaje bezpośrednio kolejkę (nie potrzebuje struktury)
    // Bo ma tylko jedno wejście: laser_queue
    xTaskCreate(Satellite::vLaserTask,
                "LaserTask",
                512,
                laser_queue,    // tutaj bezpośrednio QueueHandle_t, nie &
                                // QueueHandle_t to już wskaźnik pod spodem!
                2,
                nullptr);

    // ── INFO PRZED STARTEM ─────────────────────────────────
    std::cout << "[BOOT] Utworzono " << 5 << " tasków." << std::endl;
    std::cout << "[BOOT] Orbita: "
              << Satellite::g_sim_conditions.orbit_altitude_km << " km, "
              << Satellite::g_sim_conditions.orbit_inclination << "° inklinacja"
              << std::endl;
    std::cout << "[BOOT] Start schedulera..." << std::endl;

    // ── PRZEKAZANIE KONTROLI DO FREERTOS ──────────────────
    // Od tej linii main() przestaje być "programem".
    // FreeRTOS scheduler przejmuje CPU i wywołuje taski
    // zgodnie z priorytetami. main() nigdy tu nie wraca.
    vTaskStartScheduler();

    // ── TU NIGDY NIE POWINNIŚMY DOJŚĆ ─────────────────────
    // Jedyny powód dotarcia tutaj: brak pamięci na Idle Task
    // (FreeRTOS tworzy go automatycznie przy starcie schedulera)
    // Idle Task wymaga configMINIMAL_STACK_SIZE słów stosu.
    std::cerr << "BŁĄD KRYTYCZNY: Scheduler zatrzymany!" << std::endl;
    return 1;
}