#include "alarm.h"
#if defined(ESP_PLATFORM)
#include "rtos_objects.h"
#include "sensors.h"
#include "driver/gpio.h"
#endif

AlarmState evaluateTemperature(float temperature)
{
    if (temperature < LOW_TEMPERATURE_LIMIT) {
        return AlarmState::LOW_TEMPERATURE;
    } else if (temperature > HIGH_TEMPERATURE_LIMIT) {
        return AlarmState::HIGH_TEMPERATURE;
    } else {
        return AlarmState::NORMAL;
    }
}

#if defined(ESP_PLATFORM)
void initBuzzer(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << BUZZER_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)BUZZER_GPIO, 0);
}

void alarmTask(void *pvParameters)
{
    (void)pvParameters;
    initBuzzer();
    SensorData data;

    safeSerialPrintf("[AlarmTask] Started on Core %d, Priority %d\n", xPortGetCoreID(), uxTaskPriorityGet(NULL));

    for (;;) {
        // Block until new sensor data arrives from the queue
        if (xQueueReceive(xSensorQueueAlarm, &data, portMAX_DELAY) == pdTRUE) {
            AlarmState state = evaluateTemperature(data.temperature);

            if (state == AlarmState::NORMAL) {
                gpio_set_level((gpio_num_t)BUZZER_GPIO, 0);
                xEventGroupClearBits(xSystemEvents, EVENT_ALARM);
            } else {
                // Sound buzzer alarm and raise EVENT_ALARM bit
                gpio_set_level((gpio_num_t)BUZZER_GPIO, 1);
                xEventGroupSetBits(xSystemEvents, EVENT_ALARM);

                if (state == AlarmState::LOW_TEMPERATURE) {
                    safeSerialPrintf("[AlarmTask] WARNING: Low Temperature Alert! (%.1f C < %.1f C)\n",
                                     data.temperature, LOW_TEMPERATURE_LIMIT);
                } else {
                    safeSerialPrintf("[AlarmTask] WARNING: High Temperature Alert! (%.1f C > %.1f C)\n",
                                     data.temperature, HIGH_TEMPERATURE_LIMIT);
                }
            }
        }
    }
}
#endif
