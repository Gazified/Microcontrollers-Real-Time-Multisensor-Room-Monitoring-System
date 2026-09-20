#include "input.h"
#include "display.h"
#include "rtos_objects.h"
#include "driver/gpio.h"

void initEncoder(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << ENCODER_CLK_GPIO) | (1ULL << ENCODER_DT_GPIO) | (1ULL << ENCODER_SW_GPIO);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);
}

static const char* displayModeToString(DisplayMode mode)
{
    switch (mode) {
        case DisplayMode::TEMPERATURE: return "TEMPERATURE";
        case DisplayMode::HUMIDITY:    return "HUMIDITY";
        case DisplayMode::LIGHT:       return "LIGHT";
        case DisplayMode::MOTION:      return "MOTION";
        default:                       return "UNKNOWN";
    }
}

void inputTask(void *pvParameters)
{
    (void)pvParameters;
    initEncoder();

    safeSerialPrintf("[InputTask] Started on Core %d, Priority %d (Encoder Navigation)\n",
                     xPortGetCoreID(), uxTaskPriorityGet(NULL));

    int lastClk = gpio_get_level((gpio_num_t)ENCODER_CLK_GPIO);

    for (;;) {
        int clk = gpio_get_level((gpio_num_t)ENCODER_CLK_GPIO);
        int dt  = gpio_get_level((gpio_num_t)ENCODER_DT_GPIO);

        // Falling edge on CLK indicates rotation step
        if (lastClk == 1 && clk == 0) {
            if (dt != clk) {
                // Clockwise rotation
                gCurrentDisplayMode = nextDisplayMode(gCurrentDisplayMode);
                safeSerialPrintf("[InputTask] CW Rotation -> Display: %s\n",
                                 displayModeToString(gCurrentDisplayMode));
            } else {
                // Counter-clockwise rotation
                gCurrentDisplayMode = previousDisplayMode(gCurrentDisplayMode);
                safeSerialPrintf("[InputTask] CCW Rotation -> Display: %s\n",
                                 displayModeToString(gCurrentDisplayMode));
            }
        }
        lastClk = clk;

        // Button press resets to TEMPERATURE
        if (gpio_get_level((gpio_num_t)ENCODER_SW_GPIO) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50)); // Debounce
            if (gpio_get_level((gpio_num_t)ENCODER_SW_GPIO) == 0) {
                gCurrentDisplayMode = DisplayMode::TEMPERATURE;
                safeSerialPrintf("[InputTask] Button Pressed -> Reset to TEMPERATURE\n");
                while (gpio_get_level((gpio_num_t)ENCODER_SW_GPIO) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
            }
        }

        // Sampling interval: 10ms for snappy responsiveness without busy looping
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
