#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "WATER_LEVEL";

// ADC config
#define ADC_CHANNEL         ADC_CHANNEL_6
#define ADC_ATTEN           ADC_ATTEN_DB_12
#define NUM_SAMPLES         64

// Sensor config
#define MAX_DISTANCE_MM     1000.0
#define V_OUT_MAX_MV        5000.0

// Divider ratio (R1+R2)/R2
#define VOLTAGE_MULTIPLIER  1.5

// ========== FILTER ==========
int read_filtered_adc(adc_oneshot_unit_handle_t adc_handle) {
    int values[NUM_SAMPLES];

    for (int i = 0; i < NUM_SAMPLES; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &values[i]));
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    // sort (simple bubble sort)
    for (int i = 0; i < NUM_SAMPLES - 1; i++) {
        for (int j = i + 1; j < NUM_SAMPLES; j++) {
            if (values[i] > values[j]) {
                int tmp = values[i];
                values[i] = values[j];
                values[j] = tmp;
            }
        }
    }

    // bỏ 10% đầu + cuối (anti-noise)
    int start = NUM_SAMPLES * 0.1;
    int end   = NUM_SAMPLES * 0.9;

    int sum = 0, count = 0;
    for (int i = start; i < end; i++) {
        sum += values[i];
        count++;
    }

    return sum / count;
}

// ========== TASK ==========
void water_level_task(void *pvParameters) {

    // ADC init
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    // Channel config
    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_config));

    // Calibration (FIXED)
    adc_cali_handle_t adc_cali_handle = NULL;

    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle));

    ESP_LOGI(TAG, "ADC initialized with calibration");

    while (1) {

        int raw = read_filtered_adc(adc_handle);

        int voltage_mv = 0;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, raw, &voltage_mv));

        // restore sensor voltage
        float sensor_mv = voltage_mv * VOLTAGE_MULTIPLIER;

        // distance
        float distance_mm = (sensor_mv / V_OUT_MAX_MV) * MAX_DISTANCE_MM;

        // clamp
        if (distance_mm < 0) distance_mm = 0;
        if (distance_mm > MAX_DISTANCE_MM) distance_mm = MAX_DISTANCE_MM;

        // water level
        float water_level_mm = MAX_DISTANCE_MM - distance_mm;

        ESP_LOGI(TAG,
            "RAW=%d | V=%dmV | Sensor=%.1fmV | Dist=%.1fmm | Level=%.1fmm",
            raw, voltage_mv, sensor_mv, distance_mm, water_level_mm
        );

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
// ========== MAIN ==========
void app_main(void) {
    ESP_LOGI(TAG, "Start water level system");

    xTaskCreate(water_level_task, "water_task", 4096, NULL, 5, NULL);
}