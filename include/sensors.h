#ifndef SENSORS_H
#define SENSORS_H

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Required conceptual SensorData structure from Section 24 of Laboratory Activity No. 1
struct SensorData {
    float temperature;
    float humidity;
    int lightLevel;
    bool motionDetected;
};

#define DHT22_GPIO 4
#define LDR_GPIO   34

// Sensor initializations and acquisition functions
void initSensors(void);
bool readDHT22(float *temperature, float *humidity);
int readLDR(void);

// FreeRTOS periodic sensor acquisition task using vTaskDelayUntil()
void sensorTask(void *pvParameters);

#endif // SENSORS_H
