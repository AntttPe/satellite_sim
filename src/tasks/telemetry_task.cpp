#include "telemetry_task.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <iostream>
#include <iomanip>    // std::fixed, std::setprecision — formatowanie liczb
#include <cstdio>     // printf — czasem czytelniejsze niż cout

namespace Satellite {

void vTelemetryTask(void* pvParameters) {
    QueueHandle_t telemetry_queue = reinterpret_cast<QueueHandle_t>(pvParameters);

    std::cout << "[TELEM] Task wystartował" << std::endl;

    // Lokalny licznik pakietów — static NIE jest tu potrzebny
    // bo zmienna żyje cały czas razem z taskiem (while true nigdy nie kończy)
    uint32_t packet_id = 0;

    // SensorData na stosie taska — tu będziemy odbierać dane z kolejki
    // Ważne: struct jest na stosie (stack), nie na stercie (heap)
    // Stack = szybki, automatycznie zwalniany, ale ograniczony rozmiar
    // Heap  = wolniejszy, ręcznie zarządzany (new/delete), nieograniczony
    SensorData received_data;

    while (true) {
        // xQueueReceive(kolejka, bufor_na_dane, timeout)
        // portMAX_DELAY = czekaj w nieskończoność aż pojawi się dane
        // Task jest w stanie Blocked — nie zużywa CPU!
        // Gdy SensorTask wrzuci pakiet → kernel obudzi TelemetryTask
        BaseType_t received = xQueueReceive(
            telemetry_queue,
            &received_data,      // wskaźnik do bufora — kernel skopiuje tu dane
            portMAX_DELAY        // czekaj bez limitu
        );

        if (received != pdTRUE) {
            // To nie powinno się zdarzyć przy portMAX_DELAY
            // ale dobry kod zawsze sprawdza wartości zwracane
            continue;
        }

        ++packet_id;

        // ── Określ status systemu ─────────────────────────
        // Operator trójargumentowy: warunek ? wartość_true : wartość_false
        // Nowoczesny C++ — zwięzły zapis zamiast if/else dla prostych przypadków
        const char* status_str = (received_data.temperature > 70.0f)  ? "WARNING-TEMP" :
                                 (received_data.temperature < -30.0f) ? "WARNING-COLD" :
                                                                         "OK";

        // ── Wydrukuj pakiet telemetryczny ─────────────────
        // std::fixed + std::setprecision = zawsze 2 miejsca po przecinku
        // std::setw = minimalna szerokość pola (wyrównanie kolumn)
        std::cout << "\n[TELEM] ═══ Packet #" << packet_id << " ═══" << std::endl;
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "  Timestamp : " << received_data.timestamp_ms << " ms" << std::endl;
        std::cout << "  Status    : " << status_str << std::endl;
        std::cout << "  Temp      : " << received_data.temperature  << " °C" << std::endl;
        std::cout << "  Gyro  XYZ : "
                  << received_data.gyro_x << " | "
                  << received_data.gyro_y << " | "
                  << received_data.gyro_z << " deg/s" << std::endl;
        std::cout << "  Accel XYZ : "
                  << received_data.accel_x << " | "
                  << received_data.accel_y << " | "
                  << received_data.accel_z << " m/s²" << std::endl;
    }
}

} // namespace Satellite