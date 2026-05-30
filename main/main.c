#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "WATER_LEVEL_PROJECT";

// ================= CẤU HÌNH PHẦN CỨNG =================
#define I2C_MASTER_SCL_IO           22      
#define I2C_MASTER_SDA_IO           21      
#define I2C_MASTER_NUM              0       
#define I2C_MASTER_FREQ_HZ          50000   // FIX 1: Giảm xuống 50kHz để chống rác ký tự LCD
#define LCD_I2C_ADDR                0x27    // Nếu vẫn mù chữ thì đổi thành 0x3F nhé

#define ADC_UNIT                    ADC_UNIT_1
#define ADC_CHANNEL                 ADC_CHANNEL_6  // GPIO 34
#define ADC_ATTEN                   ADC_ATTEN_DB_12 

#define INSTALL_HEIGHT_CM           110.0   
#define SAMPLES_FOR_FILTER          20      

// ================= HÀM ĐIỀU KHIỂN LCD =================
esp_err_t i2c_master_init(void) {
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(i2c_master_port, &conf);
    return i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
}

void lcd_send_cmd(char cmd) {
    char data_u, data_l;
    uint8_t data_t[4];
    data_u = (cmd & 0xf0);
    data_l = ((cmd << 4) & 0xf0);
    data_t[0] = data_u | 0x0C;  
    data_t[1] = data_u | 0x08;  
    data_t[2] = data_l | 0x0C;  
    data_t[3] = data_l | 0x08;  
    i2c_master_write_to_device(I2C_MASTER_NUM, LCD_I2C_ADDR, data_t, 4, 1000 / portTICK_PERIOD_MS);
}

void lcd_send_data(char data) {
    char data_u, data_l;
    uint8_t data_t[4];
    data_u = (data & 0xf0);
    data_l = ((data << 4) & 0xf0);
    data_t[0] = data_u | 0x0D;  
    data_t[1] = data_u | 0x09;  
    data_t[2] = data_l | 0x0D;  
    data_t[3] = data_l | 0x09;  
    i2c_master_write_to_device(I2C_MASTER_NUM, LCD_I2C_ADDR, data_t, 4, 1000 / portTICK_PERIOD_MS);
}

void lcd_init(void) {
    // FIX 2: Tăng thời gian delay để LCD khởi động ổn định hơn
    vTaskDelay(100 / portTICK_PERIOD_MS);
    lcd_send_cmd(0x30); vTaskDelay(10 / portTICK_PERIOD_MS);
    lcd_send_cmd(0x30); vTaskDelay(2 / portTICK_PERIOD_MS);
    lcd_send_cmd(0x30); vTaskDelay(2 / portTICK_PERIOD_MS);
    lcd_send_cmd(0x20); vTaskDelay(2 / portTICK_PERIOD_MS);
    
    lcd_send_cmd(0x28); 
    lcd_send_cmd(0x0C); 
    lcd_send_cmd(0x01); 
    vTaskDelay(5 / portTICK_PERIOD_MS);
}

void lcd_set_cursor(int row, int col) {
    uint8_t address = (row == 0) ? 0x80 : 0xC0;
    address += col;
    lcd_send_cmd(address);
}

void lcd_print(char *str) {
    while (*str) {
        lcd_send_data(*str++);
    }
}

// ================= HÀM MAIN THỰC THI =================
void app_main(void) {
    ESP_LOGI(TAG, "Khoi tao I2C va LCD...");
    ESP_ERROR_CHECK(i2c_master_init());
    lcd_init();
    
    lcd_set_cursor(0, 0);
    lcd_print("Khoi dong he");
    lcd_set_cursor(1, 0);
    lcd_print("thong do luong...");
    vTaskDelay(1500 / portTICK_PERIOD_MS);
    lcd_send_cmd(0x01); vTaskDelay(2 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Khoi tao ADC...");
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = { .unit_id = ADC_UNIT };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &config));

    adc_cali_handle_t cali_handle = NULL;
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle);

    char lcd_buffer[16];
    uint32_t last_display_time = 0;

    while (1) {
        int adc_raw = 0;
        int voltage_mv = 0;
        long sum_voltage = 0;

        // FIX 3: Đọc 20 mẫu cực nhanh (chỉ mất 40ms) để nâng cao độ nhạy của cảm biến
        for (int i = 0; i < SAMPLES_FOR_FILTER; i++) {
            ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL, &adc_raw));
            adc_cali_raw_to_voltage(cali_handle, adc_raw, &voltage_mv);
            sum_voltage += voltage_mv;
            vTaskDelay(2 / portTICK_PERIOD_MS); 
        }
        
        int avg_voltage_mv = sum_voltage / SAMPLES_FOR_FILTER;
        int sensor_v_out = avg_voltage_mv * 2; 
        float distance_cm = (float)sensor_v_out * 0.06;
        float water_level_cm = INSTALL_HEIGHT_CM - distance_cm;

        if (water_level_cm < 0) water_level_cm = 0;
        if (water_level_cm > 100) water_level_cm = 100;

        // Lấy thời gian hiện tại của hệ thống (mili-giây)
        uint32_t current_time = pdTICKS_TO_MS(xTaskGetTickCount());

        // FIX 4: Hệ thống quét liên tục, nhưng chỉ In ra LCD và Log mỗi 1 giây (1000ms) một lần
        if (current_time - last_display_time >= 1000) {
            ESP_LOGI(TAG, "ADC: %d mV | Cam bien Out: %d mV | Cach: %.1f cm | Nuoc: %.1f cm", 
                     avg_voltage_mv, sensor_v_out, distance_cm, water_level_cm);

            lcd_set_cursor(0, 0);
            snprintf(lcd_buffer, sizeof(lcd_buffer), "D: %5.1f cm    ", distance_cm);
            lcd_print(lcd_buffer);

            lcd_set_cursor(1, 0);
            snprintf(lcd_buffer, sizeof(lcd_buffer), "H: %5.1f cm    ", water_level_cm);
            lcd_print(lcd_buffer);

            last_display_time = current_time;
        }

        // Delay siêu nhỏ (10ms) để hệ điều hành nhúng không bị treo
        vTaskDelay(10 / portTICK_PERIOD_MS); 
    }
}