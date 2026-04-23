#include "watchdog_task.h"       // nasza deklaracja — typy i extern semafor
#include "FreeRTOS.h"            // typy bazowe kernela
#include "task.h"                // API tasków — xTaskGetTickCount itp.
#include "semphr.h"              // API semaforów — xSemaphoreTake, xSemaphoreGive
#include <iostream>              // std::cout — wypisywanie na ekran

namespace Satellite {

// definicja globalnego semafora — tu rezerwujemy pamięć
// w .h była tylko deklaracja (extern), tu jest właściwa zmienna
// nullptr = pusty wskaźnik (nowoczesny C++, zastępuje NULL z C)
// zainicjalizujemy go w main() przez xSemaphoreCreateBinary()
SemaphoreHandle_t g_watchdog_semaphore = nullptr;

void vWatchdogTask(void* pvParameters) {

    // informacja startowa — WATCHDOG_TIMEOUT_MS to constexpr z common_types.h
    // kompilator zastąpił tę nazwę wartością 5000 już podczas kompilacji
    std::cout << "[WATCHDOG] Task wystartował, timeout: "
              << WATCHDOG_TIMEOUT_MS << "ms" << std::endl;

    uint32_t kick_count = 0;    // ile razy dostaliśmy sygnał życia od SensorTask
    uint32_t miss_count = 0;    // ile razy nie dostaliśmy (timeout)

    while (true) {              // task nigdy nie kończy — pętla nieskończona

        // czeka aż SensorTask wywoła xSemaphoreGive(g_watchdog_semaphore)
        // semafor binarny ma stan 0 lub 1
        // xSemaphoreTake: jeśli stan=1 → bierze go (stan→0), wraca natychmiast
        //                 jeśli stan=0 → czeka maksymalnie WATCHDOG_TIMEOUT_MS
        // pdMS_TO_TICKS przelicza milisekundy na ticki schedulera
        BaseType_t kicked = xSemaphoreTake(
            g_watchdog_semaphore,
            pdMS_TO_TICKS(WATCHDOG_TIMEOUT_MS)
        );

        if (kicked == pdTRUE) {     // pdTRUE = 1 = semafor otrzymany = SensorTask żyje

            ++kick_count;           // pre-inkrementacja — szybsza niż kick_count++
                                    // dla obiektów (dla uint32_t bez różnicy, ale dobry nawyk)

            // % to operator modulo — reszta z dzielenia
            // kick_count % 10 == 0 gdy kick_count jest wielokrotnością 10
            // drukujemy co 10 sygnałów żeby nie zaśmiecać wyjścia
            if (kick_count % 10 == 0) {
                std::cout << "[WATCHDOG] ✓ System OK"
                          << " | kicks: "  << kick_count
                          << " | misses: " << miss_count
                          << std::endl;
            }

        } else {                    // pdFALSE = 0 = timeout — SensorTask nie odpowiada!

            ++miss_count;

            // w prawdziwym satelicie tu byłby sprzętowy reset mikrokontrolera
            // lub przełączenie na tryb awaryjny (safe mode)
            // my tylko alarmujemy na konsoli
            std::cout << "\n[WATCHDOG] ⚠ ALARM! SensorTask nie odpowiada!"
                      << "\n[WATCHDOG] Misses: "       << miss_count
                      << " | Ostatni kick: " << kick_count
                      << std::endl;
        }
    }   // koniec while(true)
}       // koniec vWatchdogTask

} // namespace Satellite