#ifndef SOCKET_SENDER_H
#define SOCKET_SENDER_H

#include "FreeRTOS.h"
#include "queue.h"
#include "common_types.h"
#include <string>

namespace Satellite {

    // ── Parametry dla SocketSenderTask ───────────────────
    // Trzy kolejki wejściowe — zbieramy dane ze wszystkich tasków
    // i wysyłamy jako jeden skonsolidowany JSON co sekundę
    struct SocketSenderParams {
        QueueHandle_t sensor_queue;
        QueueHandle_t orbit_queue;
        QueueHandle_t laser_queue;
    };

    // ── Funkcja budująca JSON ręcznie ─────────────────────
    // Dlaczego ręcznie zamiast biblioteki?
    // W embedded unikamy dynamicznych alokacji (new/malloc).
    // nlohmann/json jest wygodny ale alokuje heap agresywnie.
    // Ręczny string jest przewidywalny i szybki.
    // To ważna lekcja embedded: kontroluj każdą alokację!
    std::string buildTelemetryJson(
        uint32_t packet_id,
        const SensorData&   sensor,
        const OrbitData&    orbit,
        const LaserLinkData& laser
    );

    void vSocketSenderTask(void* pvParameters);

} // namespace Satellite

#endif