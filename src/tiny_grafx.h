#ifndef TINYGRAFXH_
#define TINYGRAFXH_

// TINYGRAFX config
typedef struct tinygrafx_t {
  uint16_t display_width;
  uint16_t display_height;
  uint16_t display_pixel;
  uint8_t font_width;
  uint8_t font_height;
  uint8_t *display_buffer;
} tinygrafx_t;

#define BLACK   0
#define WHITE   1
#define INVERT  2

// manipulate the graphics
#define swap_int16_t(a, b) { int16_t t = a; a = b; b = t; }

void buffer_clear(tinygrafx_t tg);
void buffer_read(tinygrafx_t tg, uint8_t *data, int16_t size);
void set_pixel(tinygrafx_t tg, int16_t x, int16_t y, uint16_t color) ;
int16_t get_pixel(tinygrafx_t tg, int16_t x, int16_t y);
void draw_line(tinygrafx_t tg, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t color);
void draw_vertical_line(tinygrafx_t tg, int16_t x, int16_t y, int16_t h, int16_t color);
void draw_horizontal_line(tinygrafx_t tg, int16_t x, int16_t y, int16_t w, int16_t color);
void draw_rect(tinygrafx_t tg, int16_t x, int16_t y, int16_t w, int16_t h, int16_t color);
void draw_fill_rect(tinygrafx_t tg, int16_t x, int16_t y, int16_t w, int16_t h, int16_t color);
void draw_circle(tinygrafx_t tg, int16_t x0, int16_t y0, int16_t r, int16_t color);
void draw_fill_circle(tinygrafx_t tg, int16_t x0, int16_t y0, int16_t r, int16_t color);

// Display a character string
void draw_char(tinygrafx_t tg, int16_t x, int16_t y, uint8_t c, int16_t color, int16_t fontsize);
void display_text(tinygrafx_t tg, int16_t x, int16_t y, uint8_t *text, int16_t length, int16_t color, int16_t fontsize);

#endif /* TINYGRAFXH_ */
