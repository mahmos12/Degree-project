idf_component_register(
    SRCS
        "thingspeak.c"
        "main.c"
        "mpu6050.c"
        "i2c_dev.c"
        "ssd1306.c"
        "max30102.c"

    INCLUDE_DIRS "."

    REQUIRES
        driver
        esp_wifi
        esp_event
        nvs_flash
        esp_http_client
)