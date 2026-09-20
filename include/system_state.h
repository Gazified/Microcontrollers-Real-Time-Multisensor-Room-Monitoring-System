#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <stdint.h>
#include <stdbool.h>
#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

#define INACTIVITY_TIMEOUT_SEC 15

enum class SystemState {
    ACTIVE,
    INACTIVE
};

// Pure state decision function (Unit Test Target)
SystemState evaluateSystemState(SystemState current, bool motionDetected, uint32_t elapsedInactiveSec, uint32_t timeoutSec = INACTIVITY_TIMEOUT_SEC);

// State management task
void stateTask(void *pvParameters);

#endif // SYSTEM_STATE_H
