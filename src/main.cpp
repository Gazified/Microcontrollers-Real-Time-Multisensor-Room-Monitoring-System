#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "rtos_objects.h"
#include "system_state.h"
#include "sensors.h"
#include "display.h"
#include "input.h"
#include "alarm.h"
#include "motion.h"

extern "C" void app_main(void)
{
    // Step 1: Initialize RTOS Synchronization Objects (Queues, Mutex, Event Group)
    initRtosObjects();

    // Step 2: System Greeting
    safeSerialPrintf("=====================================================\n");
    safeSerialPrintf(" BCA152 Real-Time Multisensor Room Monitoring System \n");
    safeSerialPrintf(" ESP32 + ESP-IDF + FreeRTOS Concurrency Architecture \n");
    safeSerialPrintf(" Student Implementation - MSU-IIT CCS                \n");
    safeSerialPrintf("=====================================================\n");
    safeSerialPrintf("[System] Initializing FreeRTOS Tasks...\n");

    // Step 3: Spawn FreeRTOS Tasks with explicit priorities (Section 38 & 39)
    // Priority 3: High responsiveness (Input and Motion)
    xTaskCreatePinnedToCore(
        motionTask,
        "MotionTask",
        3072,
        NULL,
        3,
        &xMotionTaskHandle,
        1
    );

    xTaskCreatePinnedToCore(
        inputTask,
        "InputTask",
        3072,
        NULL,
        3,
        &xInputTaskHandle,
        1
    );

    // Priority 2: Periodic sensing, safety alarm, and state evaluation
    xTaskCreatePinnedToCore(
        sensorTask,
        "SensorTask",
        4096,
        NULL,
        2,
        &xSensorTaskHandle,
        0
    );

    xTaskCreatePinnedToCore(
        alarmTask,
        "AlarmTask",
        3072,
        NULL,
        2,
        &xAlarmTaskHandle,
        0
    );

    xTaskCreatePinnedToCore(
        stateTask,
        "StateTask",
        3072,
        NULL,
        2,
        &xStateTaskHandle,
        0
    );

    // Priority 1: User display rendering (can tolerate minor latency)
    xTaskCreatePinnedToCore(
        displayTask,
        "DisplayTask",
        4096,
        NULL,
        1,
        &xDisplayTaskHandle,
        1
    );

    safeSerialPrintf("[System] All 6 FreeRTOS tasks successfully spawned.\n");
    safeSerialPrintf("[System] Scheduler taking over application execution.\n\n");
}
