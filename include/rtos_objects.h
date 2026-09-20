#ifndef RTOS_OBJECTS_H
#define RTOS_OBJECTS_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "sensors.h"

// Event Group bits as required by Section 35 of the laboratory handout
#define EVENT_ACTIVE BIT0
#define EVENT_MOTION BIT1
#define EVENT_ALARM  BIT2

// RTOS Synchronization Objects
extern SemaphoreHandle_t xSerialMutex;
extern EventGroupHandle_t xSystemEvents;

// Queues: Dedicated consumer queues to avoid queue-stealing between DisplayTask and AlarmTask
extern QueueHandle_t xSensorQueueDisplay;
extern QueueHandle_t xSensorQueueAlarm;

// Task Handles for runtime inspection and state management
extern TaskHandle_t xSensorTaskHandle;
extern TaskHandle_t xDisplayTaskHandle;
extern TaskHandle_t xInputTaskHandle;
extern TaskHandle_t xMotionTaskHandle;
extern TaskHandle_t xAlarmTaskHandle;
extern TaskHandle_t xStateTaskHandle;

// Function to safely log via serial UART using the mutex
void safeSerialPrintf(const char *format, ...);

// Initialize all RTOS objects
void initRtosObjects(void);

#endif // RTOS_OBJECTS_H
