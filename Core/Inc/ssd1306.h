#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include "ll_i2c.h"

#define SSD1306_WIDTH          128
#define SSD1306_HEIGHT         64
#define SSD1306_PAGE_COUNT     8
#define SSD1306_BUFFER_SIZE    1024
#define SSD1306_DEFAULT_ADDR   0x3C

typedef enum
{
  SSD1306_COLOR_BLACK = 0,
  SSD1306_COLOR_WHITE = 1,
  SSD1306_COLOR_INVERT = 2
} SSD1306_Color_t;

typedef struct
{
  I2C_TypeDef *I2Cx;          /* Какой I2C используем: I2C1, I2C2 или I2C3. */
  uint8_t dev_adr;            /* Адрес дисплея без сдвига, обычно 0x3C. */
  uint32_t clock_speed;       /* Скорость I2C, например 100000 или 400000. */

  uint8_t buffer[SSD1306_BUFFER_SIZE];        /* Буфер экрана 128x64. */
  uint8_t transfer_buffer[SSD1306_WIDTH + 1]; /* Буфер для отправки страницы. */

  int i2c_user_init;          /* 1 - настроить I2C. */
  int display_init;           /* 1 - отправить команды инициализации SSD1306. */
  int update;                 /* 1 - отправить buffer на экран. */
  int clear;                  /* 1 - очистить buffer. */

  SSD1306_Color_t clear_color;
  uint32_t timeout_ms;
  uint8_t last_error;

  LL_I2C_Handle_t i2c;        /* Внутренний handle нижнего I2C-драйвера. */
} SSD1306_t;

void SSD1306(SSD1306_t *oled);

void SSD1306_Clear(SSD1306_t *oled, SSD1306_Color_t color);
void SSD1306_DrawPixel(SSD1306_t *oled, int16_t x, int16_t y, SSD1306_Color_t color);
void SSD1306_DrawLine(SSD1306_t *oled, int16_t x0, int16_t y0, int16_t x1, int16_t y1, SSD1306_Color_t color);
void SSD1306_DrawRect(SSD1306_t *oled, int16_t x, int16_t y, int16_t width, int16_t height, SSD1306_Color_t color);
void SSD1306_FillRect(SSD1306_t *oled, int16_t x, int16_t y, int16_t width, int16_t height, SSD1306_Color_t color);
void SSD1306_DrawEllipse(SSD1306_t *oled, int16_t cx, int16_t cy, int16_t rx, int16_t ry, SSD1306_Color_t color);
void SSD1306_DrawBitmap(SSD1306_t *oled,
                        int16_t x,
                        int16_t y,
                        const uint8_t *bitmap,
                        uint16_t width,
                        uint16_t height,
                        SSD1306_Color_t color);

#endif
