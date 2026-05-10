cmake_minimum_required(VERSION 3.20)
project(satellite_sim CXX C)

set(CMAKE_CXX_STANDARD 17)
set(FREERTOS_DIR ${CMAKE_SOURCE_DIR}/FreeRTOS)

set(FREERTOS_SOURCES
        ${FREERTOS_DIR}/tasks.c
        ${FREERTOS_DIR}/queue.c
        ${FREERTOS_DIR}/list.c
        ${FREERTOS_DIR}/timers.c
        ${FREERTOS_DIR}/portable/ThirdParty/GCC/Posix/port.c                   #Emulation for macOS
        ${FREERTOS_DIR}/portable/ThirdParty/GCC/Posix/utils/wait_for_event.c
        ${FREERTOS_DIR}/portable/MemMang/heap_3.c
)

set(APP_SOURCES
    src/main.cpp
    src/tasks/sensor_task.cpp
    src/tasks/telemetry_task.cpp
    src/tasks/watchdog_task.cpp
        src/tasks/orbit_task.cpp
        src/tasks/socket_sender.cpp
)

add_executable(satellite_sim
    ${FREERTOS_SOURCES}
    ${APP_SOURCES}
)

find_package(Threads REQUIRED)
target_link_libraries(satellite_sim Threads::Threads)

target_include_directories(satellite_sim PRIVATE
        ${FREERTOS_DIR}/include
        ${FREERTOS_DIR}/portable/ThirdParty/GCC/Posix
        ${CMAKE_SOURCE_DIR}          # FreeRTOSConfig.h jest tutaj
        ${CMAKE_SOURCE_DIR}/include  # twoje własne headery
)
