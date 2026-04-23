#define INCLUDE_xTaskDelayUntil     1

#ifndef FreeRTOS_CONFIG_H
#define FreeRTOS_CONFIG_H

// Scheduler
#define configUSE_PREEMPTION 1
// 1 - scheduler moze przerwac task
// 0 - task musiaby oddac kontrole

#define configTICK_RATE_HZ 1000
// 1000 tikow na sekudne

#define configMAX_PRIORITIES 5
// 0 - najniższy
// 4 - najwyzszy

#define configMINIMAL_STACK_SIZE 256
#define configTOTAL_HEAP_SIZE (64 * 1024) // pula pamieci

#define configMAX_TASK_NAME_LEN             16
#define configUSE_TRACE_FACILITY            1
#define configUSE_16_BIT_TICKS            0
#define configIDLE_SHOULD_YIELD             1

// Kolejki i synchornizacja
#define configUSE_MUTEXES                   1
#define configUSE_COUNTING_SEMAPHORES       1
#define configQUEUE_REGISTRY_SIZE           8
// Bezpieczne dzielenie danych miedzy taskami

// Timery
#define configUSE_TIMERS                    1
#define configTIMER_TASK_PRIORITY           ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH            10
#define configTIMER_TASK_STACK_DEPTH        256

// Bezpiecznesntwo
#define configCHECK_FOR_STACK_SIZE          2
// tryb 2 - dokladniejszy - sprawdza wzorzec bajtow na koncu stosu

// Destpene API
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskPrioritySet            1
#define INCLUDE_uxTaskPriorityGet           1
#define INCLUDE_xTaskGetHandle              1
// Wyłączenie nie uzywanych funkcji w celu optyalizacji wykorzystania zasobow

// brakujace
#define configUSE_IDLE_HOOK                 0
#define configUSE_TICK_HOOK                 0

#endif
