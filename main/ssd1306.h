#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

void ssd1306_init();
void ssd1306_clear();
void ssd1306_update();
void ssd1306_draw_pixel(int x, int y);

void ssd1306_draw_char(int x, int y, char c);
void ssd1306_draw_text(int x, int y, const char *str);

void ssd1306_draw_char_big(int x, int y, char c);
void ssd1306_draw_text_big(int x, int y, const char *str);
void ssd1306_draw_heart(int x, int y, int size);

#endif