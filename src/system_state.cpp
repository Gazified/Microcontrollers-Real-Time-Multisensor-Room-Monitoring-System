#include "system_state.h"
#if defined(ESP_PLATFORM)
#include "rtos_objects.h"
#endif

SystemState evaluateSystemState(SystemState current, bool motionDetected, uint32_t elapsedInactiveSec, uint32_t timeoutSec)
{
    if (motionDetected) {
        return SystemState::ACTIVE;
    }

    if (current == SystemState::ACTIVE) {
        if (elapsedInactiveSec >= timeoutSec) {
            return SystemState::INACTIVE;
        } else {
            return SystemState::ACTIVE;
        }
    } else {
        // Already INACTIVE and no motion
        return SystemState::INACTIVE;
    }
}

#if defined(ESP_PLATFORM)
void stateTask(void *pvParameters)
{
    (void)pvParameters;

    safeSerialPrintf("[StateTask] Started on Core %d, Priority %d (System State Manager)\n",
                     xPortGetCoreID(), uxTaskPriorityGet(NULL));

    SystemState currentState = SystemState::ACTIVE;
    uint32_t elapsedInactiveSec = 0;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Periodic 1-second cadence

        EventBits_t bits = xEventGroupGetBits(xSystemEvents);
        bool motionDetected = (bits & EVENT_MOTION) != 0;

        if (motionDetected) {
            elapsedInactiveSec = 0;
        } else {
            elapsedInactiveSec++;
        }

        SystemState newState = evaluateSystemState(currentState, motionDetected, elapsedInactiveSec, INACTIVITY_TIMEOUT_SEC);

        if (newState != currentState) {
            currentState = newState;
            if (currentState == SystemState::INACTIVE) {
                xEventGroupClearBits(xSystemEvents, EVENT_ACTIVE);
                safeSerialPrintf("[StateTask] State Change: ACTIVE -> INACTIVE (Inactivity timeout %ds reached. OLED sleeping)\n",
                                 INACTIVITY_TIMEOUT_SEC);
            } else {
                xEventGroupSetBits(xSystemEvents, EVENT_ACTIVE);
                safeSerialPrintf("[StateTask] State Change: INACTIVE -> ACTIVE (Motion detected. Restoring display)\n");
            }
        }
    }
}
#endif
