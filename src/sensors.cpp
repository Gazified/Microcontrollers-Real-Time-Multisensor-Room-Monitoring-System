#include "sensors.h"
#include "rtos_objects.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <stdio.h>

#if __has_include("esp_adc/adc_oneshot.h")
#include "esp_adc/adc_oneshot.h"
static adc_oneshot_unit_handle_t s_adc1_handle = NULL;
#elif __has_include("driver/adc.h")
#include "driver/adc.h"
#endif

#if __has_include("esp_rom_sys.h")
#include "esp_rom_sys.h"
#define delay_us esp_rom_delay_us
#else
#include "rom/ets_sys.h"
#define delay_us ets_delay_us
#endif

void initSensors(void)
{
    // Configure DHT22 data pin
    gpio_config_t dht_conf = {};
    dht_conf.intr_type = GPIO_INTR_DISABLE;
    dht_conf.mode = GPIO_MODE_OUTPUT_OD;
    dht_conf.pin_bit_mask = (1ULL << DHT22_GPIO);
    dht_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    dht_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&dht_conf);
    gpio_set_level((gpio_num_t)DHT22_GPIO, 1);

    // Configure LDR on ADC1 Channel 6 (GPIO 34)
#if __has_include("esp_adc/adc_oneshot.h")
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .clk_src = (adc_oneshot_clk_src_t)0,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    adc_oneshot_new_unit(&init_config, &s_adc1_handle);

    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_oneshot_config_channel(s_adc1_handle, ADC_CHANNEL_6, &chan_config);
#elif __has_include("driver/adc.h")
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
#endif
}

static portMUX_TYPE s_dhtMux = portMUX_INITIALIZER_UNLOCKED;

static esp_err_t awaitPinState(gpio_num_t pin, uint32_t timeout_us, int expected_state, uint32_t *duration)
{
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    for (uint32_t i = 0; i < timeout_us; i += 2) {
        delay_us(2);
        if (gpio_get_level(pin) == expected_state) {
            if (duration) *duration = i;
            return ESP_OK;
        }
    }
    return ESP_ERR_TIMEOUT;
}

bool readDHT22(float *temperature, float *humidity)
{
    uint8_t data[5] = {0, 0, 0, 0, 0};
    uint32_t low_dur = 0;
    uint32_t high_dur = 0;

    gpio_set_direction((gpio_num_t)DHT22_GPIO, GPIO_MODE_OUTPUT_OD);
    gpio_set_level((gpio_num_t)DHT22_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    // Phase A: Pull LOW for 20ms
    gpio_set_level((gpio_num_t)DHT22_GPIO, 0);
    delay_us(20000);
    gpio_set_level((gpio_num_t)DHT22_GPIO, 1);

    // Critical timing section (no task switching)
    portENTER_CRITICAL(&s_dhtMux);

    // Phase B: Wait for DHT to pull LOW (within 50us)
    if (awaitPinState((gpio_num_t)DHT22_GPIO, 50, 0, NULL) != ESP_OK) {
        portEXIT_CRITICAL(&s_dhtMux);
        gpio_set_direction((gpio_num_t)DHT22_GPIO, GPIO_MODE_OUTPUT_OD);
        gpio_set_level((gpio_num_t)DHT22_GPIO, 1);
        return false;
    }

    // Phase C: Wait for DHT to release HIGH (within 95us)
    if (awaitPinState((gpio_num_t)DHT22_GPIO, 95, 1, NULL) != ESP_OK) {
        portEXIT_CRITICAL(&s_dhtMux);
        gpio_set_direction((gpio_num_t)DHT22_GPIO, GPIO_MODE_OUTPUT_OD);
        gpio_set_level((gpio_num_t)DHT22_GPIO, 1);
        return false;
    }

    // Phase D: Wait for DHT to pull LOW to start transmission (within 95us)
    if (awaitPinState((gpio_num_t)DHT22_GPIO, 95, 0, NULL) != ESP_OK) {
        portEXIT_CRITICAL(&s_dhtMux);
        gpio_set_direction((gpio_num_t)DHT22_GPIO, GPIO_MODE_OUTPUT_OD);
        gpio_set_level((gpio_num_t)DHT22_GPIO, 1);
        return false;
    }

    // Read 40 bits
    for (int i = 0; i < 40; i++) {
        if (awaitPinState((gpio_num_t)DHT22_GPIO, 70, 1, &low_dur) != ESP_OK) {
            portEXIT_CRITICAL(&s_dhtMux);
            gpio_set_direction((gpio_num_t)DHT22_GPIO, GPIO_MODE_OUTPUT_OD);
            gpio_set_level((gpio_num_t)DHT22_GPIO, 1);
            return false;
        }

        if (awaitPinState((gpio_num_t)DHT22_GPIO, 85, 0, &high_dur) != ESP_OK) {
            portEXIT_CRITICAL(&s_dhtMux);
            gpio_set_direction((gpio_num_t)DHT22_GPIO, GPIO_MODE_OUTPUT_OD);
            gpio_set_level((gpio_num_t)DHT22_GPIO, 1);
            return false;
        }

        uint8_t b = i / 8;
        uint8_t m = i % 8;
        if (high_dur > low_dur) {
            data[b] |= (1 << (7 - m));
        }
    }

    portEXIT_CRITICAL(&s_dhtMux);

    gpio_set_direction((gpio_num_t)DHT22_GPIO, GPIO_MODE_OUTPUT_OD);
    gpio_set_level((gpio_num_t)DHT22_GPIO, 1);

    // Reject all-zero dummy reading
    if (data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 0 && data[4] == 0) {
        return false;
    }

    // Checksum verification
    uint8_t checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
    if (checksum != data[4]) {
        return false;
    }

    int16_t raw_hum = (int16_t)((data[0] << 8) | data[1]);
    int16_t raw_temp = (int16_t)(((data[2] & 0x7F) << 8) | data[3]);
    if (data[2] & 0x80) {
        raw_temp = -raw_temp;
    }

    *humidity = (float)raw_hum / 10.0f;
    *temperature = (float)raw_temp / 10.0f;
    return true;
}

int readLDR(void)
{
    int raw = 0;
#if __has_include("esp_adc/adc_oneshot.h")
    if (s_adc1_handle != NULL) {
        adc_oneshot_read(s_adc1_handle, ADC_CHANNEL_6, &raw);
    }
#elif __has_include("driver/adc.h")
    raw = adc1_get_raw(ADC1_CHANNEL_6);
#endif
    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    // Map 12-bit ADC raw (0 - 4095) to relative percentage 0 - 100%
    int percentage = (raw * 100) / 4095;
    return percentage;
}

void sensorTask(void *pvParameters)
{
    (void)pvParameters;
    initSensors();

    SensorData currentData = {
        .temperature = 24.0f,
        .humidity = 50.0f,
        .lightLevel = 50,
        .motionDetected = false
    };

    safeSerialPrintf("[SensorTask] Started on Core %d, Priority %d (2000ms periodic schedule)\n",
                     xPortGetCoreID(), uxTaskPriorityGet(NULL));

    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;) {
        float temp = 0.0f;
        float hum = 0.0f;

        if (readDHT22(&temp, &hum)) {
            currentData.temperature = temp;
            currentData.humidity = hum;
        }

        currentData.lightLevel = readLDR();

        // Check current motion status from event bit
        EventBits_t bits = xEventGroupGetBits(xSystemEvents);
        currentData.motionDetected = (bits & EVENT_MOTION) != 0;

        safeSerialPrintf("[SensorTask] Temp: %.1f C | Hum: %.1f %% | Light: %d %% | Motion: %s\n",
                         currentData.temperature,
                         currentData.humidity,
                         currentData.lightLevel,
                         currentData.motionDetected ? "YES" : "NO");

        // Fan out data to DisplayTask and AlarmTask via separate queues
        xQueueOverwrite(xSensorQueueDisplay, &currentData);
        xQueueOverwrite(xSensorQueueAlarm, &currentData);

        // Strict periodic execution to prevent time drift (Section 22 & 23)
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(2000));
    }
}
