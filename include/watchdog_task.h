#ifndef WATCHDOG_TASK_H          // include guard — jeśli już wczytany, pomiń
#define WATCHDOG_TASK_H          // zdefiniuj nazwę żeby następne wczytanie pominąć

#include "FreeRTOS.h"            // typy bazowe FreeRTOS
#include "semphr.h"              // SemaphoreHandle_t, xSemaphoreTake, xSemaphoreGive
#include "common_types.h"        // nasze stałe (WATCHDOG_TIMEOUT_MS)

namespace Satellite {            // przestrzeń nazw — chroni przed konfliktami nazw

    // extern = "ta zmienna istnieje ale jest zdefiniowana w watchdog_task.cpp"
    // g_ = konwencja dla zmiennych globalnych (global)
    // SemaphoreHandle_t = wskaźnik na strukturę semafora w kernelu FreeRTOS
    extern SemaphoreHandle_t g_watchdog_semaphore;

    // deklaracja funkcji taska — implementacja w watchdog_task.cpp
    // void* pvParameters = surowy wskaźnik, FreeRTOS tak przekazuje dane do tasków
    void vWatchdogTask(void* pvParameters);

} // namespace Satellite

#endif // WATCHDOG_TASK_H        // zamknięcie include guard