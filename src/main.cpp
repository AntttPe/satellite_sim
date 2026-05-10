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
#include "socket_sender.h"

int main() {
    std::cout << "=== Satellite Sim Boot ===" << std::endl;

    // ════════════════════════════════════════════════════════
    // KOLEJKI — komunikacja między taskami
    // ════════════════════════════════════════════════════════
    //
    // ── KLUCZOWA LEKCJA: FreeRTOS Queue to FIFO ──────────
    // Queue = jak rura — jeden wkłada, jeden wyciąga.
    // Jeśli dwóch chce CZYTAĆ z tej samej kolejki,
    // każdy pakiet trafi losowo do jednego z nich — wyścig!
    //
    // Nasze rozwiązanie: każdy ODBIORCA ma własny zestaw kolejek.
    // Producenci (SensorTask, OrbitTask, LaserTask) wysyłają
    // do DWÓCH kolejek jednocześnie — jak TV broadcast.
    //
    // Schemat przepływu danych:
    //
    //  SensorTask ──→ [telem_sensor_q] ──→ TelemetryTask (wyświetla w konsoli)
    //             └─→ [sock_sensor_q]  ──→ SocketSender  (wysyła do Python)
    //
    //  OrbitTask  ──→ [telem_orbit_q]  ──→ TelemetryTask
    //             └─→ [sock_orbit_q]   ──→ SocketSender
    //
    //  LaserTask  ──→ [telem_laser_q]  ──→ TelemetryTask
    //             └─→ [sock_laser_q]   ──→ SocketSender

    // ── Kolejki dla TelemetryTask (wyświetlanie w konsoli) ──
    QueueHandle_t telem_sensor_q = xQueueCreate(10, sizeof(Satellite::SensorData));
    QueueHandle_t telem_orbit_q  = xQueueCreate(5,  sizeof(Satellite::OrbitData));
    QueueHandle_t telem_laser_q  = xQueueCreate(10, sizeof(Satellite::LaserLinkData));

    if (!telem_sensor_q || !telem_orbit_q || !telem_laser_q) {
        std::cerr << "BŁĄD: brak pamięci na kolejki TelemetryTask!" << std::endl;
        return 1;
    }

    // ── Kolejki dla SocketSender (wysyłanie JSON do Python) ─
    // Osobne od powyższych! Każdy pakiet musi trafić do OBU odbiorców.
    QueueHandle_t sock_sensor_q = xQueueCreate(10, sizeof(Satellite::SensorData));
    QueueHandle_t sock_orbit_q  = xQueueCreate(5,  sizeof(Satellite::OrbitData));
    QueueHandle_t sock_laser_q  = xQueueCreate(10, sizeof(Satellite::LaserLinkData));

    if (!sock_sensor_q || !sock_orbit_q || !sock_laser_q) {
        std::cerr << "BŁĄD: brak pamięci na kolejki SocketSender!" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════
    // SEMAFOR WATCHDOGA
    // ════════════════════════════════════════════════════════
    //
    // Semafor binarny = przełącznik z dwoma stanami: 0 i 1.
    // SensorTask co 100ms robi xSemaphoreGive() → stan 1
    // WatchdogTask robi xSemaphoreTake(timeout=5000ms) → stan 0
    //
    // Jeśli SensorTask padnie → WatchdogTask nie dostanie Give()
    // → xSemaphoreTake() przekroczy timeout → ALARM!
    //
    // To klasyczny "heartbeat" pattern w systemach krytycznych.
    // Samoloty, rakiety, satelity — wszędzie działa tak samo.
    Satellite::g_watchdog_semaphore = xSemaphoreCreateBinary();
    if (Satellite::g_watchdog_semaphore == nullptr) {
        std::cerr << "BŁĄD: brak pamięci na semafor watchdoga!" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════
    // STRUKTURY PARAMETRÓW DLA TASKÓW
    // ════════════════════════════════════════════════════════
    //
    // ── DLACZEGO static? ─────────────────────────────────
    // xTaskCreate przyjmuje void* pvParameters — surowy wskaźnik.
    // Task będzie używał tego wskaźnika długo po wyjściu z main().
    //
    // Zwykła (non-static) zmienna lokalna:
    //   int x = 5;           ← żyje na stosie main()
    //   &x → wskaźnik ważny tylko wewnątrz main()
    //   Po wyjściu z main() → dangling pointer! (wskaźnik na śmieci)
    //
    // Static zmienna lokalna:
    //   static int x = 5;    ← żyje w sekcji .data (pamięć globalna)
    //   &x → wskaźnik ważny przez CAŁY czas działania programu
    //   Bezpieczne do przekazania do taska!
    //
    // Uwaga: static local = inicjalizowana TYLKO RAZ (przy pierwszym wywołaniu)
    // Tu to nie problem bo main() wywołujemy raz.

    // ── Parametry SensorTask ──────────────────────────────
    // SensorTask musi pisać do DWÓCH kolejek jednocześnie.
    // Dlatego używamy SensorTaskParams zamiast przekazywać
    // jedną kolejkę bezpośrednio.
    static Satellite::SensorTaskParams sensor_params = {
        .telem_queue  = telem_sensor_q,  // → TelemetryTask
        .socket_queue = sock_sensor_q    // → SocketSender
    };

    // ── Parametry OrbitTask ───────────────────────────────
    static Satellite::OrbitTaskParams orbit_params = {
        .output_queue    = telem_orbit_q,  // → TelemetryTask
        .socket_queue    = sock_orbit_q,   // → SocketSender
        .conditions      = &Satellite::g_sim_conditions
    };

    // ── Parametry LaserTask ───────────────────────────────
    // LaserTask też musi pisać do dwóch kolejek.
    static Satellite::LaserTaskParams laser_params = {
        .telem_queue  = telem_laser_q,
        .socket_queue = sock_laser_q
    };

    // ── Parametry TelemetryTask ───────────────────────────
    static Satellite::TelemetryTaskParams telem_params = {
        .sensor_queue = telem_sensor_q,
        .orbit_queue  = telem_orbit_q,
        .laser_queue  = telem_laser_q
    };

    // ── Parametry SocketSender ────────────────────────────
    static Satellite::SocketSenderParams socket_params = {
        .sensor_queue = sock_sensor_q,
        .orbit_queue  = sock_orbit_q,
        .laser_queue  = sock_laser_q
    };

    // ════════════════════════════════════════════════════════
    // TWORZENIE TASKÓW
    // ════════════════════════════════════════════════════════
    //
    // xTaskCreate(funkcja, nazwa, stos, parametry, priorytet, uchwyt)
    //
    // ── Rozmiar stosu — WAŻNA UWAGA ──────────────────────
    // Trzeci argument to rozmiar stosu w SŁOWACH (words), nie bajtach!
    // Na arm64: 1 word = 8 bajtów
    // 512 words = 4096 bajtów = 4KB
    // 2048 words = 16384 bajtów = 16KB
    //
    // Za mały stos = stack overflow = crash bez wyraźnego błędu!
    // Za duży stos = marnowanie pamięci (mamy tylko 64KB heap w FreeRTOS)
    // SocketSender ma 2048 bo std::string i ostringstream
    // alokują tymczasowe bufory na stosie taska.
    //
    // ── Priorytety w naszym systemie ─────────────────────
    // 3 = Watchdog    — NAJWYŻSZY, musi zawsze działać
    // 2 = SensorTask  — dane IMU krytyczne czasowo (co 100ms)
    // 2 = OrbitTask   — fizyka orbity (co 1s)
    // 2 = LaserTask   — symulacja łącza (co 500ms)
    // 1 = TelemTask   — wyświetlanie, może poczekać
    // 1 = SocketSender — I/O sieciowe, może poczekać
    //
    // Scheduler FreeRTOS: zawsze uruchamia task z NAJWYŻSZYM
    // priorytetem który jest gotowy do działania (nie zablokowany).
    // Taski o tym samym priorytecie dzielą CPU round-robin.

    xTaskCreate(Satellite::vSensorTask,
                "SensorTask",
                512,
                &sensor_params,  // teraz przekazujemy strukturę, nie samą kolejkę
                2,
                nullptr);

    xTaskCreate(Satellite::vOrbitTask,
                "OrbitTask",
                1024,            // cmath (sin/cos/sqrt) potrzebuje miejsca na stosie
                &orbit_params,
                2,
                nullptr);

    xTaskCreate(Satellite::vLaserTask,
                "LaserTask",
                512,
                &laser_params,   // teraz struktura zamiast pojedynczej kolejki
                2,
                nullptr);

    xTaskCreate(Satellite::vTelemetryTask,
                "TelemTask",
                1024,
                &telem_params,
                1,
                nullptr);

    xTaskCreate(Satellite::vSocketSenderTask,
                "SocketSender",
                2048,            // duży stos — ostringstream + std::string
                &socket_params,
                1,
                nullptr);

    xTaskCreate(Satellite::vWatchdogTask,
                "Watchdog",
                256,             // mały stos — watchdog robi tylko Take() na semaforze
                nullptr,         // nie potrzebuje parametrów — używa globalnego semafora
                3,               // NAJWYŻSZY priorytet — nikt go nie zagłodzi
                nullptr);

    // ════════════════════════════════════════════════════════
    // BOOT INFO I START SCHEDULERA
    // ════════════════════════════════════════════════════════

    std::cout << "[BOOT] Utworzono 6 tasków." << std::endl;
    std::cout << "[BOOT] Orbita: "
              << Satellite::g_sim_conditions.orbit_altitude_km << " km, "
              << Satellite::g_sim_conditions.orbit_inclination << "° inklinacja"
              << std::endl;
    std::cout << "[BOOT] Start schedulera..." << std::endl;

    // ── vTaskStartScheduler() — punkt bez powrotu ─────────
    // Od tej chwili FreeRTOS przejmuje pełną kontrolę nad CPU.
    // main() przestaje być "programem głównym" — staje się
    // zwykłym kawałkiem pamięci który scheduler może nadpisać.
    //
    // Co się dzieje wewnątrz vTaskStartScheduler():
    //   1. Tworzy Idle Task (priorytet 0) — działa gdy nic innego nie chce
    //   2. Tworzy Timer Task jeśli włączony w config
    //   3. Włącza przerwania systemowe (tick interrupt)
    //   4. Wchodzi w nieskończoną pętlę schedulera
    //   5. NIGDY nie wraca
    vTaskStartScheduler();

    // ── Tu dochodzimy TYLKO przy błędzie krytycznym ───────
    // Jedyna możliwa przyczyna: brak pamięci na Idle Task.
    // configTOTAL_HEAP_SIZE w FreeRTOSConfig.h musi być
    // wystarczająco duży na wszystkie taski + Idle Task.
    std::cerr << "BŁĄD KRYTYCZNY: Scheduler zatrzymany!" << std::endl;
    return 1;
}