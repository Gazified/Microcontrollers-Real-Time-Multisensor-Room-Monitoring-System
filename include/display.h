#ifndef DISPLAY_H
#define DISPLAY_H

#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

// Required display mode enumeration from Section 28 of the laboratory handout
enum class DisplayMode {
    TEMPERATURE,
    HUMIDITY,
    LIGHT,
    MOTION
};

// Pure navigation functions with wraparound in both directions (Unit Test Target)
DisplayMode nextDisplayMode(DisplayMode current);
DisplayMode previousDisplayMode(DisplayMode current);

// Current display mode controlled by InputTask
extern DisplayMode gCurrentDisplayMode;

#define OLED_I2C_SDA_GPIO 21
#define OLED_I2C_SCL_GPIO 22
#define OLED_I2C_NUM      0
#define SSD1306_I2C_ADDR  0x3C

// OLED hardware init and ownership task
void initOLED(void);
void displayTask(void *pvParameters);

#endif // DISPLAY_H
