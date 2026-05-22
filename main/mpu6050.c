#include "mpu6050.h"
#include "i2c_dev.h"

#define MPU_ADDR 0x68

// Initialize MPU6050 sensor
void mpu6050_init() {
    // Wake up device (disable sleep mode)
    i2c_write(MPU_ADDR, 0x6B, 0);
}

// Read accelerometer X, Y and Z values
bool mpu6050_read(int16_t *x, int16_t *y, int16_t *z){

    uint8_t data[6];

    // Read accelerometer registers
    if (i2c_read(MPU_ADDR, 0x3B, data, 6) != ESP_OK) {
        return false;
    }

    // Convert raw bytes to signed 16-bit values
    *x = (data[0] << 8) | data[1];
    *y = (data[2] << 8) | data[3];
    *z = (data[4] << 8) | data[5];

    return true;
}