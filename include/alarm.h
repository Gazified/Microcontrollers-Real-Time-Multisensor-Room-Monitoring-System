#ifndef ALARM_H
#define ALARM_H

#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

// Temperature limits specified in Laboratory Activity No. 1, p. 3
#define LOW_TEMPERATURE_LIMIT  18.0f
#define HIGH_TEMPERATURE_LIMIT 30.0f
#define BUZZER_GPIO            23

enum class AlarmState {
    NORMAL,
    LOW_TEMPERATURE,
    HIGH_TEMPERATURE
};

// Pure decision logic separated from hardware control (for unit testing)
AlarmState evaluateTemperature(float temperature);

// Alarm task entry point
void alarmTask(void *pvParameters);

// Hardware initialization for the buzzer
void initBuzzer(void);

#endif // ALARM_H
