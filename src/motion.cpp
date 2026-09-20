#include "motion.h"
#include "rtos_objects.h"
#include "driver/gpio.h"

void initMotion(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << PIR_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
}

void motionTask(void *pvParameters)
{
    (void)pvParameters;
    initMotion();

    safeSerialPrintf("[MotionTask] Started on Core %d, Priority %d (PIR Monitor)\n",
                     xPortGetCoreID(), uxTaskPriorityGet(NULL));

    bool lastState = false;

    for (;;) {
        bool currentState = (gpio_get_level((gpio_num_t)PIR_GPIO) == 1);

        if (currentState != lastState) {
            lastState = currentState;
            if (currentState) {
                // Motion detected: set EVENT_MOTION and re-activate system
                xEventGroupSetBits(xSystemEvents, EVENT_MOTION | EVENT_ACTIVE);
                safeSerialPrintf("[MotionTask] >>> Motion DETECTED! System awakened to ACTIVE <<<\n");
            } else {
                // Motion cleared
                xEventGroupClearBits(xSystemEvents, EVENT_MOTION);
                safeSerialPrintf("[MotionTask] Motion ended (room clear).\n");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
