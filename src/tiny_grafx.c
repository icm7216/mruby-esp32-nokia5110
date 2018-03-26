// ===================================================================
//
//    Tiny graphics libraries for SSD1306 (for esp-idf)
//
// ===================================================================
//
// The MIT License
//
// Copyright (c) 2018 icm7216
//
// Permission is hereby granted, free of charge, to any person 
// obtaining a copy of this software and associated documentation 
// files (the "Software"), to deal in the Software without 
// restriction, including without limitation the rights to use, 
// copy, modify, merge, publish, distribute, sublicense, and/or 
// sell copies of the Software, and to permit persons to whom the 
// Software is furnished to do so, subject to the following 
// conditions:
//
// The above copyright notice and this permission notice shall be 
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR 
// OTHER DEALINGS IN THE SOFTWARE.
//
// ===================================================================


#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
static const char *TAG = "TINY_GRAFX";

// 8x8 monochrome bitmap fonts from font8x8_basic.h by dhepper/font8x8
// https://github.com/dhepper/font8x8
#include "font8x8_basic.h"

#include "tiny_grafx.h"

// manipulate graphics
//
// This graphics libraries are adapted from OLEDDisplay.cpp by squix78/esp8266-oled-ssd1306.
// https://github.com/squix78/esp8266-oled-ssd1306
//


void 
buffer_clear(tinygrafx_t tg) 
{
  memset(tg.display_buffer, 0x00, tg.display_pixel);
}

void 
buffer_read(tinygrafx_t tg, uint8_t *data, int16_t size) 
{
  if (data == NULL) {
    ESP_LOGI(TAG, "buffer_read: data NULL error");
  }
  if (size == tg.display_pixel) {
    for (int16_t i=0; i<tg.display_pixel; i++) {
      data[i] = tg.display_buffer[i];
    }
  }
  else {
    ESP_LOGI(TAG, "buffer_read: data size mismatch => %d", size);
  }
}

void 
set_pixel(tinygrafx_t tg, int16_t x, int16_t y, uint16_t color) 
{
  if ((x >= 0) && (x < tg.display_width) && (y >= 0) && (y < tg.display_height)) {
    switch (color) {
      case WHITE: tg.display_buffer[x + (y / 8) * tg.display_width] |=  (1 << (y & 7)); break;
      case BLACK: tg.display_buffer[x + (y / 8) * tg.display_width] &= ~(1 << (y & 7)); break;
      case INVERT:tg.display_buffer[x + (y / 8) * tg.display_width] ^=  (1 << (y & 7)); break;
    }
  } 
}

int16_t 
get_pixel(tinygrafx_t tg, int16_t x, int16_t y) 
{
  if ((x >= 0) && (x < tg.display_width) && (y >= 0) && (y < tg.display_height)) {
    return (tg.display_buffer[x + (y / 8) * tg.display_width] >> (y % 8)) & 0x1;
  }
  else {
    return 0;
  }
}

void 
draw_line(tinygrafx_t tg, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t color) 
{
  int16_t steep = abs(y1 - y0) > abs(x1 - x0);

  if (steep) {
    swap_int16_t(x0, y0);
    swap_int16_t(x1, y1);
  }
  if (x0 > x1) {
    swap_int16_t(x0, x1);
    swap_int16_t(y0, y1);
  }

  int16_t dx = x1 - x0;
  int16_t dy = abs(y1 - y0);
  int16_t err = dx / 2;
  int16_t ystep;

  if (y0 < y1) {
    ystep = 1;
  }
  else {
    ystep = -1;
  }

  for (; x0 <= x1; x0++) {
    if (steep) {
      set_pixel(tg, y0, x0, color);
    }
    else {
      set_pixel(tg, x0, y0, color);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}

void 
draw_vertical_line(tinygrafx_t tg, int16_t x, int16_t y, int16_t h, int16_t color) 
{
  if (y < 0) {
    h += y;
    y = 0;
  }

  if ((y + h) > tg.display_height) {
    h = tg.display_height - y; 
  }

  if (h <= 0) return;
  
  for (int16_t h1 = 0; h1 < h; h1++) {
    set_pixel(tg, x, y + h1, color);
  }
}

void 
draw_horizontal_line(tinygrafx_t tg, int16_t x, int16_t y, int16_t w, int16_t color) 
{
  if (x < 0) {
    w += x;
    x = 0;
  }

  if ((x + w) > tg.display_width) {
    w = tg.display_width - x; 
  }

  if (w <= 0) return;
  
  for (int16_t w1 = 0; w1 < w; w1++) {
    set_pixel(tg, x + w1, y, color);
  }
}

void 
draw_rect(tinygrafx_t tg, int16_t x, int16_t y, int16_t w, int16_t h, int16_t color) 
{
  draw_horizontal_line(tg, x, y, w, color);
  draw_horizontal_line(tg, x, y + h - 1, w, color);
  draw_vertical_line(tg, x, y, h, color);
  draw_vertical_line(tg, x + w - 1, y, h, color);
}

void 
draw_fill_rect(tinygrafx_t tg, int16_t x, int16_t y, int16_t w, int16_t h, int16_t color) 
{
  for (int16_t w1 = 0; w1 < w; w1++) {
    draw_vertical_line(tg, x + w1, y, h, color);
  }
}

void 
draw_circle(tinygrafx_t tg, int16_t x0, int16_t y0, int16_t r, int16_t color) 
{
  int16_t x = 0;
  int16_t y = r;
	int16_t dp = 1 - r;

  set_pixel(tg, x0, y0 + r, color);
  set_pixel(tg, x0, y0 - r, color);
  set_pixel(tg, x0 + r, y0, color);
  set_pixel(tg, x0 - r, y0, color);

	do {
		if (dp < 0) {
			dp = dp + 2 * (++x) + 3;
    }
    else {
			dp = dp + 2 * (++x) - 2 * (--y) + 5;
    }

		set_pixel(tg, x0 + x, y0 + y, color);     //For the 8 octants
		set_pixel(tg, x0 - x, y0 + y, color);
		set_pixel(tg, x0 + x, y0 - y, color);
		set_pixel(tg, x0 - x, y0 - y, color);
		set_pixel(tg, x0 + y, y0 + x, color);
		set_pixel(tg, x0 - y, y0 + x, color);
		set_pixel(tg, x0 + y, y0 - x, color);
		set_pixel(tg, x0 - y, y0 - x, color);

	} while (x < y);
}

void 
draw_fill_circle(tinygrafx_t tg, int16_t x0, int16_t y0, int16_t r, int16_t color) 
{
  int16_t x = 0;
  int16_t y = r;
	int16_t dp = 1 - r;
  
  draw_horizontal_line(tg, x0 - r, y0, 2 * r, color);

	do {
		if (dp < 0) {
			dp = dp + 2 * (++x) + 3;
    }
    else {
			dp = dp + 2 * (++x) - 2 * (--y) + 5;
    }

    draw_horizontal_line(tg, x0 - x, y0 - y, 2 * x, color);
    draw_horizontal_line(tg, x0 - x, y0 + y, 2 * x, color);
    draw_horizontal_line(tg, x0 - y, y0 - x, 2 * y, color);
    draw_horizontal_line(tg, x0 - y, y0 + x, 2 * y, color);
	} while (x < y);
}

// Display a character string
void 
draw_char(tinygrafx_t tg, int16_t x, int16_t y, uint8_t c, int16_t color, int16_t fontsize) 
{
  uint8_t row_pixel;
  uint16_t font_width;

  for (int16_t y1 = 0; y1 < tg.font_height; y1++) {  
    row_pixel = font8x8_basic[c][y1];

    for (int16_t x1 = 0; x1 < tg.font_width; x1++) {
      if (row_pixel & 0x01) {
        if (fontsize == 1) {
          set_pixel(tg, x + x1, y + y1, color);
        }
        else {
          font_width = (fontsize & 0x01) + (fontsize / 2);
          draw_fill_rect(tg, x + x1 * font_width, y + y1 * fontsize, font_width, fontsize, color);
        }
      }
      row_pixel >>= 1;
    }
  }
  // ESP_LOGI(TAG, "draw char: 0x%X=%c", c, c);
}

void 
display_text(tinygrafx_t tg, int16_t x, int16_t y, uint8_t *text, int16_t length, int16_t color, int16_t fontsize) 
{
  // ESP_LOGI(TAG, "display text: %s, length: %d, fontsize: %d", text, length, fontsize);
  uint16_t font_width;

  for (int16_t i = 0; i < length; i++) {
    if (text[i] == '\n') {
      x =0;
      y += tg.font_width * fontsize;
    }
    else {
      draw_char(tg, x, y, text[i], color, fontsize);
      if (fontsize == 1) {
        x += tg.font_width * fontsize;
      }
      else {
        font_width = (fontsize & 0x01) + (fontsize / 2);
        x += tg.font_width * font_width;
      }
    }
  }
}

