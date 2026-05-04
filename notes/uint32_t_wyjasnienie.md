# C++ / FreeRTOS: Typ uint32_t i zmienna timestamp_ms

## 1. Dlaczego `uint32_t` (zamiast zwykłego `int`)?
W systemach wbudowanych (ang. *embedded*) i protokołach radiowych (np. satelitarna telemetria) kluczowa jest kontrola nad rozmiarem danych:

*   **Zwykły `int`:** Jego rozmiar (np. 16, 32 lub 64 bity) zależy od kompilatora i architektury (np. 8-bitowy AVR vs 32-bitowy ARM).
*   **`uint32_t`:** Zawsze i wszędzie zajmuje dokładnie **32 bity (4 bajty)**.
    *   **u** = *unsigned* (bez znaku, tylko wartości dodatnie). Czasu i tak nie mierzy się w ujemnych, więc podwajamy górny limit zmiennej.
    *   **32** = rozmiar w bitach.
    *   **_t** = konwencja oznaczająca typ (*type*).

Używanie `<cstdint>` zapewnia stabilność rozmiaru pakietów (np. `struct OrbitData` lub `TelemetryPacket`), co ułatwia m.in. de/serializację bajtów.

## 2. Rola `timestamp_ms` (Stempel Czasowy)
W systemie klasy RTOS czas upływa za sprawą tzw. "ticków" schedulera (zwykle konfigurowanych na 1 tick = 1 milisekunda).

*   Zmienna ta przechowuje **dokładny moment wygenerowania danych** (liczony od włączenia urządzenia i startu schedulera `vTaskStartScheduler()`).
*   We FreeRTOS do pobrania tej wartości używa się najczęściej `xTaskGetTickCount()`.

### Po co dodajemy czas (timestamp) do pomiarów?
1.  **Kontekst dla danych:** W przypadku satelity lecącego np. 7.5 km/s, same współrzędne bez precyzyjnego czasu są bezużyteczne. `timestamp_ms` łączy dane pozycyjne z konkretną chwilą.
2.  **Kolejkowanie (Queues):** Pakiety mogą czekać w kolejkach FreeRTOS (np. `telemetry_queue`). Timestamp informuje odbiorcę, jak "stary" jest odebrany pomiar.

### ⚠️ Uwaga projektowa: Przepełnienie (Overflow)
Z racji tego, że `uint32_t` mieści maks. **4 294 967 295**, zliczając milisekundy ulegnie on przepełnieniu (wróci do zera) po około **49,7 dnia**. Systemy pracujące w kosmosie muszą ten przypadek niezawodnie obsługiwać (programowo)!