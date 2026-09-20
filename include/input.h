#ifndef INPUT_H
#define INPUT_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ENCODER_CLK_GPIO 18
#define ENCODER_DT_GPIO  19
#define ENCODER_SW_GPIO  5

void initEncoder(void);
void inputTask(void *pvParameters);

#endif // INPUT_H
