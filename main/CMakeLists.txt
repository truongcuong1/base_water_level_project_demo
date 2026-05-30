# Đăng ký component main, biên dịch file main.c, include thư mục hiện tại
# và YÊU CẦU CẤP QUYỀN TRUY CẬP vào thư viện 'driver' (cho I2C) và 'esp_adc' (cho cảm biến)
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_adc)