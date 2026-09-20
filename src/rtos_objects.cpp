#include "rtos_objects.h"
#include <stdio.h>
#include <stdarg.h>

SemaphoreHandle_t xSerialMutex = NULL;
EventGroupHandle_t xSystemEvents = NULL;

QueueHandle_t xSensorQueueDisplay = NULL;
QueueHandle_t xSensorQueueAlarm = NULL;

TaskHandle_t xSensorTaskHandle = NULL;
TaskHandle_t xDisplayTaskHandle = NULL;
TaskHandle_t xInputTaskHandle = NULL;
TaskHandle_t xMotionTaskHandle = NULL;
TaskHandle_t xAlarmTaskHandle = NULL;
TaskHandle_t xStateTaskHandle = NULL;

void safeSerialPrintf(const char *format, ...)
{
    if (xSerialMutex != NULL) {
        if (xSemaphoreTake(xSerialMutex, portMAX_DELAY) == pdTRUE) {
            va_list args;
            va_start(args, format);
            vprintf(format, args);
            va_end(args);
            fflush(stdout);
            xSemaphoreGive(xSerialMutex);
        }
    } else {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        fflush(stdout);
    }
}

void initRtosObjects(void)
{
    // Create mutex for protecting serial UART stdout
    xSerialMutex = xSemaphoreCreateMutex();

    // Create event group for system-wide flags
    xSystemEvents = xEventGroupCreate();
    // Default to active state upon startup
    xEventGroupSetBits(xSystemEvents, EVENT_ACTIVE);

    // Create fan-out queues for SensorData (length 5 each)
    xSensorQueueDisplay = xQueueCreate(5, sizeof(SensorData));
    xSensorQueueAlarm = xQueueCreate(5, sizeof(SensorData));
}
