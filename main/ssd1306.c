#include "ssd1306.h"
#include "i2c_dev.h"
#include <string.h>

#define OLED_ADDR 0x3C

// Display frame buffer (128x64 pixels)
static uint8_t buffer[128 * 64 / 8];

// Send command to SSD1306
void ssd1306_cmd(uint8_t cmd){

    uint8_t data[2] = {0x00, cmd};
    i2c_master_write_to_device(I2C_NUM_0, OLED_ADDR, data, 2, 1000 / portTICK_PERIOD_MS);
}

// Send display data to SSD1306
void ssd1306_data(uint8_t *data, size_t len){

    uint8_t temp[len + 1];
    temp[0] = 0x40;
    memcpy(&temp[1], data, len);

    i2c_master_write_to_device(I2C_NUM_0, OLED_ADDR, temp, len + 1, 1000 / portTICK_PERIOD_MS);
}

// Initialize OLED display
void ssd1306_init(){

    ssd1306_cmd(0xAE);

    ssd1306_cmd(0x20);
    ssd1306_cmd(0x00);

    ssd1306_cmd(0xB0);
    ssd1306_cmd(0xC8);
    ssd1306_cmd(0x00);
    ssd1306_cmd(0x10);
    ssd1306_cmd(0x40);

    ssd1306_cmd(0x81); ssd1306_cmd(0xFF);
    ssd1306_cmd(0xA1);
    ssd1306_cmd(0xA6);
    ssd1306_cmd(0xA8); ssd1306_cmd(0x3F);
    ssd1306_cmd(0xA4);
    ssd1306_cmd(0xD3); ssd1306_cmd(0x00);
    ssd1306_cmd(0xD5); ssd1306_cmd(0xF0);
    ssd1306_cmd(0xD9); ssd1306_cmd(0x22);
    ssd1306_cmd(0xDA); ssd1306_cmd(0x12);
    ssd1306_cmd(0xDB); ssd1306_cmd(0x20);
    ssd1306_cmd(0x8D); ssd1306_cmd(0x14);

    // Turn display on
    ssd1306_cmd(0xAF);
}

// Clear frame buffer
void ssd1306_clear(){

    memset(buffer, 0, sizeof(buffer));
}

// Transfer frame buffer to display
void ssd1306_update(){

    ssd1306_cmd(0x21);
    ssd1306_cmd(0);
    ssd1306_cmd(127);

    ssd1306_cmd(0x22);
    ssd1306_cmd(0);
    ssd1306_cmd(7);

    for (int i = 0; i < 8; i++){

        ssd1306_data(&buffer[i * 128], 128);
    }
}

// Draw a single pixel in the frame buffer
void ssd1306_draw_pixel(int x, int y){

    int page = y / 8;
    int bit  = y % 8;
    int index = page * 128 + x;

    if (index < sizeof(buffer)){

        buffer[index] |= (1 << bit);
    }
}

/* 5x7 FONT */
static const uint8_t font5x7[][5] = {
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},

    {0x3E,0x45,0x49,0x51,0x3E},{0x00,0x21,0x7F,0x01,0x00},
    {0x21,0x43,0x45,0x49,0x31},{0x42,0x41,0x51,0x69,0x46},
    {0x0C,0x14,0x24,0x7F,0x04},{0x72,0x51,0x51,0x51,0x4E},
    {0x1E,0x29,0x49,0x49,0x06},{0x40,0x47,0x48,0x50,0x60},
    {0x36,0x49,0x49,0x49,0x36},{0x30,0x49,0x49,0x4A,0x3C},
};

// Draw a scaled character (2x size)
void ssd1306_draw_char_big(int x, int y, char c){

    const uint8_t *ch = 0;

    if (c >= 'A' && c <= 'Z') ch = font5x7[c - 'A'];
    else if (c >= '0' && c <= '9') ch = font5x7[26 + (c - '0')];

    if (!ch) return;

    for (int col = 0; col < 5; col++){
        uint8_t line = ch[col];

        // Flip digit orientation if needed
        if (c >= '0' && c <= '9'){

            uint8_t reversed = 0;

            for (int i = 0; i < 8; i++){

                if (line & (1 << i)) reversed |= (1 << (7 - i));
            }

            line = reversed;
        }

        for (int row = 0; row < 8; row++){

            if (line & (1 << row)){

                int px = x + col * 2;
                int py = y + row * 2;

                // Draw 2x2 pixel block
                ssd1306_draw_pixel(px, py);
                ssd1306_draw_pixel(px + 1, py);
                ssd1306_draw_pixel(px, py + 1);
                ssd1306_draw_pixel(px + 1, py + 1);
            }
        }
    }
}

// Draw a string using large characters
void ssd1306_draw_text_big(int x, int y, const char *str){
    while (*str){

        ssd1306_draw_char_big(x, y, *str);
        x += 12;
        str++;
    }
}

// Draw a simple heart icon
void ssd1306_draw_heart(int x, int y, int size){

    for (int i = 0; i < size; i++){

        for (int j = 0; j < size; j++){
            int dx = i - size / 2;
            int dy = j - size / 2;

            if ((dx * dx + dy * dy < (size * size) / 5) || 
            ((dx - size / 3) * (dx - size / 3) + dy * dy < (size * size) / 5) ||
            (dy > 0 && abs(dx) < size / 2 - dy)){
                
                ssd1306_draw_pixel(x + i, y + j);
            }
        }
    }
}