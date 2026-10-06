#include "alarm.h"
#if defined(ESP_PLATFORM)
#include "rtos_objects.h"
#include "sensors.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#endif

#if defined(ESP_PLATFORM)
static void setBuzzerTone(bool active)
{
    if (active) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 4096);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    } else {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
}
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
    ledc_timer_config_t timer_conf = {};
    timer_conf.speed_mode = LEDC_LOW_SPEED_MODE;
    timer_conf.duty_resolution = LEDC_TIMER_13_BIT;
    timer_conf.timer_num = LEDC_TIMER_0;
    timer_conf.freq_hz = 2000;
    timer_conf.clk_cfg = LEDC_AUTO_CLK;
    timer_conf.deconfigure = false;

    ledc_timer_config(&timer_conf);

    ledc_channel_config_t ch_conf = {};
    ch_conf.gpio_num = BUZZER_GPIO;
    ch_conf.speed_mode = LEDC_LOW_SPEED_MODE;
    ch_conf.channel = LEDC_CHANNEL_0;
    ch_conf.timer_sel = LEDC_TIMER_0;
    ch_conf.duty = 0;
    ch_conf.hpoint = 0;
    ch_conf.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD;
    ch_conf.flags.output_invert = 0;
    ch_conf.deconfigure = false;

    ledc_channel_config(&ch_conf);
    setBuzzerTone(false);
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
                setBuzzerTone(false);
                xEventGroupClearBits(xSystemEvents, EVENT_ALARM);
            } else {
                // Sound buzzer alarm and raise EVENT_ALARM bit
                setBuzzerTone(true);
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
