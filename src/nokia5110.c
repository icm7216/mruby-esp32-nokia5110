#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/string.h>
#include <mruby/value.h>
#include <mruby/variable.h>
#include <mruby/data.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"

#include "tiny_grafx.h"

// PCD8544 function set
#define PCD8544_FUNCTIONSET     0x20
#define PCD8544_POWERDOWN       0x04
#define PCD8544_VADDRMODE       0x02
#define PCD8544_EXTINSTRUCTION  0x01
#define PCD8544_DISPLAYBLANK    0x0
#define PCD8544_DISPLAYNORMAL   0x4
#define PCD8544_DISPLAYALLON    0x1
#define PCD8544_DISPLAYINVERSE  0x5

// PCD8544 basic instruction set
#define PCD8544_DISPLAYCTRL     0x08
#define PCD8544_SETYADDR        0x40
#define PCD8544_SETXADDR        0x80

// PCD8544 extended instruction set
#define PCD8544_SETTEMP         0x04
#define PCD8544_SETBIAS         0x10
#define PCD8544_SETVOP          0x80

// PCD8544 display config
#define PCD8544_DISPLAY_WIDTH   84
#define PCD8544_DISPLAY_HEIGHT  48
#define PCD8544_DISPLAY_PIXEL   504
#define PCD8544_FONT_WIDTH      8
#define PCD8544_FONT_HEIGHT     8 

// D/C pin mode, command or data
enum {
    DC_CMD,
    DC_DATA
};

// DMA channel
#define NO_DMA  0
#define DMA_CH1 1
#define DMA_CH2 2

// NO_DMA mode transaction data size is up to 32 bytes at a time.
#define NO_DMA_TRANSACTION_DATA_SIZE 32 

// default pcd8544 wiring and SPI configuration
#define PCD8544_PIN_NUM_CS   5
#define PCD8544_PIN_NUM_DC   16
#define PCD8544_PIN_NUM_RST  17
#define PCD8544_PIN_NUM_MOSI 23
#define PCD8544_PIN_NUM_SCK  18
#define PCD8544_PIN_NUM_MISO 19
#define PCD8544_CLOCK_SPEED_HZ (4*1000*1000)   // SPI Clock freq=4 MHz
#define PCD8544_SPI_MODE 0
#define PCD8544_DMA DMA_CH1                    // default DMA channel = 1

// SPI HOST, only HSPI or VSPI
#define PCD8544_HOST VSPI_HOST

// SPI Object start
typedef struct spi_config_t {
  uint8_t num_cs;           // Chip Select pin num
  uint8_t num_dc;           // Data/Command select pin num
  uint8_t num_rst;          // RESET pin num
  uint8_t num_mosi;         // MOSI pin num
  uint8_t num_sck;          // SPI Clock pin num
  uint8_t num_miso;         // MISO pin num
  uint32_t spi_freq;        // SPI clock frequency [Hz]
  uint8_t spi_mode;         // SPI mode (0-3)
  uint8_t dma_ch;           // No DMA or DMA channel (1 or 2)
  bool require_reset;       // Reset the display
  spi_device_handle_t spi;  // Handle for a device on a SPI bus
  tinygrafx_t tinygrafx;    // Tiny graphics config and frame buffer
} spi_config_t;

static const char *TAG = "PCD8544";


// ----- Common graphics methods ----------
// mruby binding of manipulate the graphics
// ----------------------------------------
static mrb_value
lcd_clear(mrb_state *mrb, mrb_value self)
{
  spi_config_t *tg = (spi_config_t *)DATA_PTR(self);

  buffer_clear(tg->tinygrafx);
  return self;
}

static mrb_value
lcd_set_pixel(mrb_state *mrb, mrb_value self)
{
	mrb_int x, y;
  int16_t color;
  spi_config_t *tg = (spi_config_t *)DATA_PTR(self);
  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  mrb_get_args(mrb, "ii", &x, &y);
	
  set_pixel(tg->tinygrafx, x, y, color);
  return mrb_nil_value();
}

static mrb_value
lcd_get_pixel(mrb_state *mrb, mrb_value self)
{
	mrb_int x, y;
  int16_t pixel;
  spi_config_t *tg = (spi_config_t *)DATA_PTR(self);
  mrb_get_args(mrb, "ii", &x, &y);
	
  pixel = get_pixel(tg->tinygrafx, x, y);
  return mrb_fixnum_value(pixel);
}

static mrb_value
lcd_draw_line(mrb_state *mrb, mrb_value self)
{
  mrb_int x0, y0, x1, y1;
  int16_t color;
  spi_config_t *tg = (spi_config_t *)DATA_PTR(self);
  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  mrb_get_args(mrb, "iiii", &x0, &y0, &x1, &y1);
  if ((color < BLACK) || (color > INVERT)) {
    color = WHITE;
  }
  
  draw_line(tg->tinygrafx, x0, y0, x1, y1, color);
  return mrb_nil_value();
}

static mrb_value
lcd_draw_vertical_line(mrb_state *mrb, mrb_value self)
{
	mrb_int x, y, h;
  int16_t color;
  spi_config_t *tg = (spi_config_t *)DATA_PTR(self);
  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  mrb_get_args(mrb, "iii", &x, &y, &h);
	
  draw_vertical_line(tg->tinygrafx, x, y, h, color);
  return mrb_nil_value();
}

static mrb_value
lcd_draw_horizontal_line(mrb_state *mrb, mrb_value self)
{
	mrb_int x, y, w;
  int16_t color;
  spi_config_t *tg = (spi_config_t *)DATA_PTR(self);
  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  mrb_get_args(mrb, "iii", &x, &y, &w);
	
  draw_horizontal_line(tg->tinygrafx, x, y, w, color);
	return mrb_nil_value();
}

static mrb_value
lcd_draw_rect(mrb_state *mrb, mrb_value self)
{
	mrb_int x, y, w, h;
  int16_t color;
  spi_config_t *tg = (spi_config_t *)DATA_PTR(self);
  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  mrb_get_args(mrb, "iiii", &x, &y, &w, &h);
	
  draw_rect(tg->tinygrafx, x, y, w, h, color);
	return mrb_nil_value();
}

static mrb_value
lcd_draw_fill_rect(mrb_state *mrb, mrb_value self)
{
	mrb_int x, y, w, h;
  int16_t color;
  spi_config_t *tg = (spi_config_t *)DATA_PTR(self);
  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  mrb_get_args(mrb, "iiii", &x, &y, &w, &h);
	
  draw_fill_rect(tg->tinygrafx, x, y, w, h, color);
	return mrb_nil_value();
}

static mrb_value
lcd_draw_circle(mrb_state *mrb, mrb_value self)
{
	mrb_int x, y, r;
  int16_t color;
  spi_config_t *tg = (spi_config_t *)DATA_PTR(self);
  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  mrb_get_args(mrb, "iii", &x, &y, &r);
	
  draw_circle(tg->tinygrafx, x, y, r, color);
	return mrb_nil_value();
}

static mrb_value
lcd_draw_fill_circle(mrb_state *mrb, mrb_value self)
{
  mrb_int x, y, r;
  int16_t color;
  spi_config_t *tg = (spi_config_t *)DATA_PTR(self);
  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  mrb_get_args(mrb, "iii", &x, &y, &r);
	
  draw_fill_circle(tg->tinygrafx, x, y, r, color);
	return mrb_nil_value();
}

// mruby binding of Display a character string
static mrb_value
lcd_text(mrb_state *mrb, mrb_value self)
{
  mrb_int x, y;
  mrb_value data;
  int16_t color, fontsize;
  spi_config_t *tg = (spi_config_t *)DATA_PTR(self);
  color = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@color")));
  fontsize = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@fontsize")));
  mrb_get_args(mrb, "iiS", &x, &y, &data);
  
  display_text(tg->tinygrafx, x, y, RSTRING_PTR(data), RSTRING_LEN(data), color, fontsize);
  // ESP_LOGI(TAG, "color:%d, size:%d, text:%s", color, fontsize, RSTRING_PTR(data));
  return mrb_nil_value();
}
// ----- Common graphics methods -----




// ----- PCD8544 methods and functions -----


/* -----------------------------------------------------------
    Note: The following two functions are not used. 
    Because this function cannot specify any other arguments.
   ----------------------------------------------------------- */
// // This function is called (in irq context!) just before a transmission starts.
// // It will set the D/C line to the value indicated in the user field.
// static void
// spi_pre_transfer_callback(spi_transaction_t *t)
// {
//   uint32_t dc = (uint32_t)t->user;
//   gpio_set_level(PCD8544_PIN_NUM_DC, dc);
// }
// // reset the D/C line
// static void
// spi_post_transfer_callback(spi_transaction_t *t)
// {
//   gpio_set_level(PCD8544_PIN_NUM_DC, 0);
// }

// Send buffer data to the display
// NOTE: NO_DMA mode can transmit up to 32 bytes at a time.
static void
send_data(spi_config_t *spicfg, const uint8_t *data, int16_t len, int32_t dc)
{
  esp_err_t err;
  spi_transaction_t tx;

  // spi pre-transfer setting, control lines.
  gpio_set_level(spicfg->num_cs, 0);
  gpio_set_level(spicfg->num_dc, dc);

  if (spicfg->dma_ch == 0) {
    // NO_DMA mode
    int16_t max_len, tx_len, left_len;
    void *cur_data = (void *)data;
    max_len = NO_DMA_TRANSACTION_DATA_SIZE;
    left_len = len;
    // Split transmission with 32 bytes
    while (left_len > 0) {
      tx_len = (left_len > max_len) ? max_len : left_len;
      memset(&tx, 0, sizeof(tx));
      tx.length = tx_len * 8;   // tx_len is in bytes, transaction length is in bits.
      tx.tx_buffer = cur_data;  // Transmit data
      tx.user = (void*)dc;      // D/C needs to be set to 1
      err = spi_device_queue_trans(spicfg->spi, &tx, 1000 / portTICK_PERIOD_MS);
      if (err != ESP_OK) {
        ESP_LOGI(TAG, "send_data: spi_device_queue_trans error=%d", err);
      }
      spi_transaction_t *rx;
      err = spi_device_get_trans_result(spicfg->spi, &rx, 1000 / portTICK_PERIOD_MS);
      if (err != ESP_OK) {
        ESP_LOGI(TAG, "send_data: spi_device_get_trans_result error=%d", err);
      }
      left_len -= tx_len;
      cur_data += tx_len;
    }
  } else {
    // Use DMA mode
    memset(&tx, 0, sizeof(tx));
    tx.length = len * 8;        // len is in bytes, transaction length is in bits.
    tx.tx_buffer = data;        // Transmit data
    tx.user = (void*)dc;        // D/C needs to be set to 1
    err = spi_device_queue_trans(spicfg->spi, &tx, 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
      ESP_LOGI(TAG, "send_data: spi_device_queue_trans error=%d", err);
    }
    spi_transaction_t *rx;
    err = spi_device_get_trans_result(spicfg->spi, &rx, 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
      ESP_LOGI(TAG, "send_data: spi_device_get_trans_result error=%d", err);
    }
  }
  // spi post-transfer setting, control lines.
  gpio_set_level(spicfg->num_dc, 0);
  gpio_set_level(spicfg->num_cs, 1);
}

// write address config. use horizontal addressing mode.
DRAM_ATTR static const uint8_t pcd8544_address_init[] = {
  (PCD8544_FUNCTIONSET),                        // use basic instruction set
  (PCD8544_DISPLAYCTRL|PCD8544_DISPLAYNORMAL),  // display config = normal mode
  (PCD8544_SETXADDR),                           // set X address = 0
  (PCD8544_SETYADDR)                            // set Y address = 0
};

// Send buffer to display
static void
pcd8544_send_display(spi_config_t *spicfg)
{
  uint8_t *buffer;

  if (spicfg->dma_ch == 0) {
    // NO DMA
    buffer = (uint8_t *)malloc(spicfg->tinygrafx.display_pixel);
  } else {
    // Use DMA_CH1 or DMA_CH2
    buffer = (uint8_t *)heap_caps_malloc(spicfg->tinygrafx.display_pixel, MALLOC_CAP_DMA);
  }

  if (buffer != NULL) {
    memset(buffer, 0x00, spicfg->tinygrafx.display_pixel);
    buffer_read(spicfg->tinygrafx, buffer, spicfg->tinygrafx.display_pixel);
    // send buffer to pcd8544
    send_data(spicfg, pcd8544_address_init, sizeof(pcd8544_address_init), DC_CMD);
    send_data(spicfg, buffer, spicfg->tinygrafx.display_pixel, DC_DATA);
  }

  if (spicfg->dma_ch == 0) {
    free(buffer);
  } else {
    heap_caps_free(buffer);
  }
}

// display the frame buffer
static mrb_value
pcd8544_spi_display(mrb_state *mrb, mrb_value self)
{
  spi_config_t *spicfg = (spi_config_t *)DATA_PTR(self);
  pcd8544_send_display(spicfg);
  return mrb_nil_value();
}

// Initialize the SPI manter
static void
spi_bus_init(spi_config_t *spicfg)
{
  spi_bus_config_t buscfg = {
    .miso_io_num = spicfg->num_miso,
    .mosi_io_num = spicfg->num_mosi,
    .sclk_io_num = spicfg->num_sck,
    .quadwp_io_num = -1,  // WP (Write Protect) signal, or -1 if not used.
    .quadhd_io_num = -1   // HD (HolD) signal, or -1 if not used.
  };
  spi_device_interface_config_t devcfg = {
    .clock_speed_hz = spicfg->spi_freq,
    .mode = spicfg->spi_mode,
    .spics_io_num = spicfg->num_cs,
    .queue_size = 1,
    .pre_cb = NULL,     // spi_pre_transfer_callback,
    .post_cb = NULL     // spi_post_transfer_callback
  };
  esp_err_t err;
  spi_device_handle_t spi;
  
  // Initialize the SPI bus
  err = spi_bus_initialize(PCD8544_HOST, &buscfg, spicfg->dma_ch);
  spicfg->require_reset = (err == ESP_ERR_INVALID_STATE) ? false : true;
  if (err != ESP_OK) {
    ESP_LOGI(TAG, "spi_bus_init: spi_bus_initialize status=%d", err);
  }

  // Attach the OLED to the SPI bus
  err = spi_bus_add_device(PCD8544_HOST, &devcfg, &spi);
  if (err != ESP_OK) {
    ESP_LOGI(TAG, "spi_bus_init: spi_bus_add_device status=%d", err);
  }
  // Save the SPI device handle to SPI Object.
  spicfg->spi = spi;
}

static void
spi_deinit(spi_config_t *spicfg)
{
  free(spicfg->tinygrafx.display_buffer);
  free(spicfg->spi);
}

// pcd8544 init commands
DRAM_ATTR static const uint8_t pcd8544_init_cmds[] = {
  (PCD8544_FUNCTIONSET|PCD8544_EXTINSTRUCTION), // use extended instruction set
  (PCD8544_SETTEMP|0x00),                       // set temperature coefficient
  (PCD8544_SETBIAS|0x03),                       // set bias system
  (PCD8544_SETVOP|0x39)                         // set contrast
};

// pcd8544 Initialize
static void
pcd8544_init(spi_config_t *spicfg)
{
  // Initialize non-SPI GPIOs
  gpio_set_direction(spicfg->num_dc, GPIO_MODE_OUTPUT);
  gpio_set_direction(spicfg->num_rst, GPIO_MODE_OUTPUT);
  gpio_set_direction(spicfg->num_cs, GPIO_MODE_OUTPUT);
  gpio_set_pull_mode(spicfg->num_cs, GPIO_PULLUP_ONLY);

  // Reset the display if host not in use
  if (spicfg->require_reset) {
    gpio_set_level(spicfg->num_rst, 1);
    gpio_set_level(spicfg->num_rst, 0);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    gpio_set_level(spicfg->num_rst, 1);
  }

  // Send all commands
  send_data(spicfg, pcd8544_init_cmds, sizeof(pcd8544_init_cmds), DC_CMD);
}

// Configuration the Tiny graphics libraries
static void
tinygrafx_init(spi_config_t *spicfg)
{
  tinygrafx_t tg = {
    .display_width = PCD8544_DISPLAY_WIDTH,
    .display_height = PCD8544_DISPLAY_HEIGHT,
    .display_pixel = PCD8544_DISPLAY_PIXEL,
    .font_width = PCD8544_FONT_WIDTH,
    .font_height = PCD8544_FONT_HEIGHT
  }; 
  // set frame buffer
  uint8_t *buffer;
  buffer = (uint8_t *)malloc(tg.display_pixel);
  if (buffer != NULL) {
    memset(buffer, 0, tg.display_pixel);
  }
  tg.display_buffer = buffer; 

  spicfg->tinygrafx = tg;
}

// free mrb object for GC.
static void
meb_pcd8544_free(mrb_state *mrb, void *ptr)
{
  spi_config_t *spicfg = ptr;
  mrb_free(mrb, spicfg->tinygrafx.display_buffer);
  mrb_free(mrb, spicfg->spi);
}

// mruby data_type
static const struct mrb_data_type mrb_spi_config_type = {
  "spi_config_type", meb_pcd8544_free
};

// Initialize the SPI device for pcd8544
static mrb_value
pcd8544_spi_init(mrb_state *mrb, mrb_value self)
{
  spi_config_t *spicfg = (spi_config_t *)DATA_PTR(self);
  if (spicfg) {
    mrb_free(mrb, spicfg);
  }
  DATA_PTR(self) = NULL;

  // Get config param
  mrb_int cs, dc, rst, mosi, sck, miso, freq, spi_mode, dma_ch;
  mrb_get_args(mrb, "iiiiiiiii", &cs, &dc, &rst, &mosi, &sck, &miso, &freq, &spi_mode, &dma_ch);

  // pcd8544 SPI bus config
  spicfg = (spi_config_t *)mrb_malloc(mrb, sizeof(spi_config_t));
  spicfg->num_cs   = cs;
  spicfg->num_dc   = dc;
  spicfg->num_rst  = rst;
  spicfg->num_mosi = mosi;
  spicfg->num_sck  = sck;
  spicfg->num_miso = miso;
  spicfg->spi_freq = freq;
  spicfg->spi_mode = spi_mode;
  spicfg->dma_ch   = dma_ch;
  DATA_TYPE(self) = &mrb_spi_config_type;
  DATA_PTR(self)  = spicfg;

  // Initialize the SPI
  spi_bus_init(spicfg);
  
  // Initialize the pcd8544
  pcd8544_init(spicfg);
  
  // Initialize the TINYGRAFX
  tinygrafx_init(spicfg);
  
  return self;
}


// // Object duplication method
// static mrb_value
// spi_init_copy(mrb_state *mrb, mrb_value copy)
// {
//   mrb_value src;
//   mrb_get_args(mrb, "o", &src);
//   if (mrb_obj_equal(mrb, copy, src)) return copy;
//   if (!mrb_obj_is_instance_of(mrb, src, mrb_obj_class(mrb, copy))) {
//     mrb_raise(mrb, E_TYPE_ERROR, "wrong argument class");
//   }
//   if (!DATA_PTR(copy)) {
//     DATA_PTR(copy) = (spi_config_t *)mrb_malloc(mrb, sizeof(spi_config_t));
//     DATA_TYPE(copy) = &mrb_spi_config_type;
//   }
//   *(spi_config_t *)DATA_PTR(copy) = *(spi_config_t *)DATA_PTR(src);
//   return copy;
// }

// View config setting
static mrb_value
spi_view_config(mrb_state *mrb, mrb_value self)
{
  spi_config_t *spicfg = DATA_PTR(self);
  mrb_value spi_param = mrb_ary_new_capa(mrb, 9);
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->num_cs));
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->num_dc));
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->num_rst));
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->num_mosi));
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->num_sck));
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->num_miso));
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->spi_freq));
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->spi_mode));
  mrb_ary_push(mrb, spi_param, mrb_fixnum_value(spicfg->dma_ch));
  return spi_param;
}

// Set contrast
static mrb_value
pcd8544_contrast(mrb_state *mrb, mrb_value self)
{
  mrb_int contrast;
  spi_config_t *spicfg = (spi_config_t *)DATA_PTR(self);
  mrb_get_args(mrb, "i", &contrast);
  contrast = (contrast > 0x7F) ? 0x7F : contrast; // Range of contrast values (0-127)
  uint8_t pcd8544_contrast_cmds[] = {
    (PCD8544_FUNCTIONSET|PCD8544_EXTINSTRUCTION),
    (PCD8544_SETVOP|contrast)
  };
  send_data(spicfg, pcd8544_contrast_cmds, sizeof(pcd8544_contrast_cmds), DC_CMD);
  return mrb_nil_value();
}


void
mrb_mruby_esp32_nokia5110_gem_init(mrb_state* mrb)
{
  struct RClass *lcd = mrb_define_module(mrb, "LCD");
  mrb_define_const(mrb, lcd, "BLACK", mrb_fixnum_value(BLACK));
  mrb_define_const(mrb, lcd, "WHITE", mrb_fixnum_value(WHITE));
  mrb_define_const(mrb, lcd, "INVERT", mrb_fixnum_value(INVERT));

  struct RClass *pcd8544 = mrb_define_class_under(mrb, lcd, "NOKIA5110", mrb->object_class);
  MRB_SET_INSTANCE_TT(pcd8544, MRB_TT_DATA);

  // Common graphics methods
  mrb_define_method(mrb, pcd8544, "clear", lcd_clear, MRB_ARGS_NONE());
  mrb_define_method(mrb, pcd8544, "set_pixel", lcd_set_pixel, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, pcd8544, "get_pixel", lcd_get_pixel, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, pcd8544, "line", lcd_draw_line, MRB_ARGS_REQ(4));
  mrb_define_method(mrb, pcd8544, "vline", lcd_draw_vertical_line, MRB_ARGS_REQ(3));
  mrb_define_method(mrb, pcd8544, "hline", lcd_draw_horizontal_line, MRB_ARGS_REQ(3));
  mrb_define_method(mrb, pcd8544, "rect", lcd_draw_rect, MRB_ARGS_REQ(4));
  mrb_define_method(mrb, pcd8544, "fill_rect", lcd_draw_fill_rect, MRB_ARGS_REQ(4));
  mrb_define_method(mrb, pcd8544, "circle", lcd_draw_circle, MRB_ARGS_REQ(3));
  mrb_define_method(mrb, pcd8544, "fill_circle", lcd_draw_fill_circle, MRB_ARGS_REQ(3));
  mrb_define_method(mrb, pcd8544, "text", lcd_text, MRB_ARGS_REQ(3));

  // Send frame buffer to display
  mrb_define_method(mrb, pcd8544, "display", pcd8544_spi_display, MRB_ARGS_NONE());

  // pcd8544 spi method
  mrb_define_method(mrb, pcd8544, "_init", pcd8544_spi_init, MRB_ARGS_NONE());
  // mrb_define_method(mrb, pcd8544, "initialize_copy", spi_init_copy, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, pcd8544, "config?", spi_view_config, MRB_ARGS_NONE());

  // pcd8544 control command
  mrb_define_method(mrb, pcd8544, "contrast=", pcd8544_contrast, MRB_ARGS_REQ(1));

  struct RClass *constants = mrb_define_module_under(mrb, pcd8544, "Constants");
  mrb_define_const(mrb, constants, "CS",        mrb_fixnum_value(PCD8544_PIN_NUM_CS));
  mrb_define_const(mrb, constants, "DC",        mrb_fixnum_value(PCD8544_PIN_NUM_DC));
  mrb_define_const(mrb, constants, "RST",       mrb_fixnum_value(PCD8544_PIN_NUM_RST));
  mrb_define_const(mrb, constants, "MOSI",      mrb_fixnum_value(PCD8544_PIN_NUM_MOSI));
  mrb_define_const(mrb, constants, "SCK",       mrb_fixnum_value(PCD8544_PIN_NUM_SCK));
  mrb_define_const(mrb, constants, "MISO",      mrb_fixnum_value(PCD8544_PIN_NUM_MISO));
  mrb_define_const(mrb, constants, "SPI_FREQ",  mrb_fixnum_value(PCD8544_CLOCK_SPEED_HZ));
  mrb_define_const(mrb, constants, "SPI_MODE",  mrb_fixnum_value(PCD8544_SPI_MODE));
  mrb_define_const(mrb, constants, "DMA",       mrb_fixnum_value(PCD8544_DMA));
  mrb_define_const(mrb, constants, "NO_DMA",    mrb_fixnum_value(NO_DMA));
  mrb_define_const(mrb, constants, "DMA_CH1",   mrb_fixnum_value(DMA_CH1));
  mrb_define_const(mrb, constants, "DMA_CH2",   mrb_fixnum_value(DMA_CH2));
}

void
mrb_mruby_esp32_nokia5110_gem_final(mrb_state* mrb)
{
}

