#ifndef I2C_DEV_H
#define I2C_DEV_H


//oled

#include "driver/i2c.h"
#include "esp_err.h"

void i2c_master_init();
esp_err_t i2c_write(uint8_t addr, uint8_t reg, uint8_t data);
esp_err_t i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);


#endif