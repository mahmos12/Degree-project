#include "i2c_dev.h"


#define I2C_MASTER_SCL_IO 9
#define I2C_MASTER_SDA_IO 8
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000

// Initialize I2C master
void i2c_master_init(){
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

// Write one byte to a device register
esp_err_t i2c_write(uint8_t addr, uint8_t reg, uint8_t data){
    uint8_t buf[2] = {reg, data};

    return i2c_master_write_to_device(I2C_MASTER_NUM, addr, buf, 2, pdMS_TO_TICKS(20));
}

// Read data from a device register
esp_err_t i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len){
    
    return i2c_master_write_read_device(I2C_MASTER_NUM,addr,&reg,1,data,len,pdMS_TO_TICKS(20));
}
