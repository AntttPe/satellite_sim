#ifndef TELEMETRY_TASK_H
#define TELEMETRY_TASK_H

#include "FreeRTOS.h"
#include "queue.h"
#include "common_types.h"

namespace Satellite {

    void vTelemetryTask(void *pvParameters);

} // namespace Satellite

#endif