#ifndef MPU6050_H
#define MPU6050_H

//Motion sensor
#include <stdint.h>
#include <stdbool.h>

void mpu6050_init();
bool mpu6050_read(int16_t *x, int16_t *y, int16_t *z);

#endif