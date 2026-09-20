#ifndef MOTION_H
#define MOTION_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PIR_GPIO 13

void initMotion(void);
void motionTask(void *pvParameters);

#endif // MOTION_H
