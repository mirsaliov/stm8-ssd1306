#include "ssd1306.h"
#include <string.h>

#define SSD1306_CTRL_COMMAND 0x00
#define SSD1306_CTRL_DATA    0x40

static void ssd1306_i2c_init(SSD1306_t *oled);
static unsigned char ssd1306_cmd(SSD1306_t *oled, uint8_t cmd);
static unsigned char ssd1306_data(SSD1306_t *oled, const uint8_t *data, uint16_t size);
static void ssd1306_display_init(SSD1306_t *oled);
static void ssd1306_update(SSD1306_t *oled);
static void ssd1306_update_page(SSD1306_t *oled, uint8_t page);
static int16_t ssd1306_abs16(int16_t value);

/* Команды начальной настройки SSD1306 128x64. */
static const uint8_t ssd1306_init_sequence[] =
{
  0xAE,       /* выключить дисплей */
  0x20, 0x00, /* горизонтальная адресация */
  0xB0,       /* начальная страница */
  0xC8,       /* направление COM */
  0x00,       /* младшая часть адреса колонки */
  0x10,       /* старшая часть адреса колонки */
  0x40,       /* начальная строка */
  0x81, 0x7F, /* контраст */
  0xA1,       /* разворот сегментов */
  0xA6,       /* обычный режим, не инверсия */
  0xA8, 0x3F, /* multiplex 1/64 */
  0xA4,       /* выводить содержимое RAM */
  0xD3, 0x00, /* смещение дисплея */
  0xD5, 0x80, /* частота дисплея */
  0xD9, 0xF1, /* pre-charge */
  0xDA, 0x12, /* COM pins */
  0xDB, 0x40, /* VCOMH */
  0x8D, 0x14, /* charge pump */
  0xAF        /* включить дисплей */
};

void SSD1306(SSD1306_t *oled)
{
  if (!oled)
  {
    return;
  }

  /*
   * Функция сделана как в примере EEPROM:
   * пользователь выставляет флаг в структуре, вызывает SSD1306(),
   * а драйвер выполняет нужное действие и сбрасывает флаг.
   */

  if (oled->i2c_user_init == 1)
  {
    ssd1306_i2c_init(oled);
    oled->i2c_user_init = 0;
    return;
  }

  if (oled->display_init == 1)
  {
    ssd1306_display_init(oled);
    oled->display_init = 0;
    return;
  }

  if (oled->clear == 1)
  {
    SSD1306_Clear(oled, oled->clear_color);
    oled->clear = 0;
    return;
  }

  if (oled->update == 1)
  {
    ssd1306_update(oled);
    oled->update = 0;
    return;
  }
}

static void ssd1306_i2c_init(SSD1306_t *oled)
{
  LL_I2C_Config_t cfg = {0};

  if (!oled || !oled->I2Cx)
  {
    return;
  }

  cfg.instance = oled->I2Cx;
  cfg.clock_speed = oled->clock_speed;
  cfg.own_address1 = 0;
  cfg.gpio_pull = 1;   /* pull-up */
  cfg.gpio_speed = 3;  /* very high speed */
  cfg.auto_recovery = 1;

  /*
   * Для простоты пины выбираются автоматически по номеру I2C.
   * I2C1: PB6  - SCL, PB7  - SDA
   * I2C2: PB10 - SCL, PB11 - SDA
   * I2C3: PA8  - SCL, PC9  - SDA
   */
  if (oled->I2Cx == I2C1)
  {
    cfg.scl.port = GPIOB;
    cfg.scl.pin = (1U << 6);
    cfg.scl.alternate = 4;
    cfg.sda.port = GPIOB;
    cfg.sda.pin = (1U << 7);
    cfg.sda.alternate = 4;
  }
  else if (oled->I2Cx == I2C2)
  {
    cfg.scl.port = GPIOB;
    cfg.scl.pin = (1U << 10);
    cfg.scl.alternate = 4;
    cfg.sda.port = GPIOB;
    cfg.sda.pin = (1U << 11);
    cfg.sda.alternate = 4;
  }
  else if (oled->I2Cx == I2C3)
  {
    cfg.scl.port = GPIOA;
    cfg.scl.pin = (1U << 8);
    cfg.scl.alternate = 4;
    cfg.sda.port = GPIOC;
    cfg.sda.pin = (1U << 9);
    cfg.sda.alternate = 4;
  }
  else
  {
    oled->last_error = 1;
    return;
  }

  if (LL_I2C_Init(&oled->i2c, &cfg) != LL_I2C_OK)
  {
    oled->last_error = 2;
  }
}

static void ssd1306_display_init(SSD1306_t *oled)
{
  if (!oled)
  {
    return;
  }

  if (oled->dev_adr == 0)
  {
    oled->dev_adr = SSD1306_DEFAULT_ADDR;
  }
  if (oled->timeout_ms == 0)
  {
    oled->timeout_ms = 100;
  }

  SSD1306_Clear(oled, SSD1306_COLOR_BLACK);

  for (uint32_t i = 0; i < sizeof(ssd1306_init_sequence); i++)
  {
    if (!ssd1306_cmd(oled, ssd1306_init_sequence[i]))
    {
      return;
    }
  }

  ssd1306_update(oled);
}

static unsigned char ssd1306_cmd(SSD1306_t *oled, uint8_t cmd)
{
  return ssd1306_data(oled, &cmd, 1);
}

static unsigned char ssd1306_data(SSD1306_t *oled, const uint8_t *data, uint16_t size)
{
  uint8_t control;
  LL_I2C_Status_t status;

  if (!oled || !data || size == 0 || size > SSD1306_WIDTH)
  {
    return 0;
  }

  /*
   * Если отправляем один байт через ssd1306_cmd(), это команда.
   * Если отправляем страницу экрана, это данные.
   */
  control = (size == 1) ? SSD1306_CTRL_COMMAND : SSD1306_CTRL_DATA;

  oled->transfer_buffer[0] = control;
  memcpy(&oled->transfer_buffer[1], data, size);

  /*
   * В нижний драйвер адрес передается уже сдвинутым на 1 бит.
   * Поэтому 0x3C превращается в 0x78.
   */
  status = LL_I2C_MasterTransmit(&oled->i2c,
                                 (uint16_t)(oled->dev_adr << 1),
                                 oled->transfer_buffer,
                                 (uint16_t)(size + 1),
                                 oled->timeout_ms);

  if (status != LL_I2C_OK)
  {
    oled->last_error = 3;
    return 0;
  }

  oled->last_error = 0;
  return 1;
}

static void ssd1306_update(SSD1306_t *oled)
{
  if (!oled)
  {
    return;
  }

  /*
   * SSD1306 делит экран 128x64 на 8 страниц.
   * В каждой странице 128 байт.
   */
  for (uint8_t page = 0; page < SSD1306_PAGE_COUNT; page++)
  {
    ssd1306_update_page(oled, page);
  }
}

static void ssd1306_update_page(SSD1306_t *oled, uint8_t page)
{
  uint16_t offset;

  if (!oled)
  {
    return;
  }

  /* Выбираем страницу и первую колонку. */
  if (!ssd1306_cmd(oled, (uint8_t)(0xB0 + page))) return;
  if (!ssd1306_cmd(oled, 0x00)) return;
  if (!ssd1306_cmd(oled, 0x10)) return;

  offset = (uint16_t)page * SSD1306_WIDTH;
  (void)ssd1306_data(oled, &oled->buffer[offset], SSD1306_WIDTH);
}

void SSD1306_Clear(SSD1306_t *oled, SSD1306_Color_t color)
{
  if (!oled)
  {
    return;
  }

  if (color == SSD1306_COLOR_WHITE)
  {
    memset(oled->buffer, 0xFF, SSD1306_BUFFER_SIZE);
  }
  else
  {
    memset(oled->buffer, 0x00, SSD1306_BUFFER_SIZE);
  }
}

void SSD1306_DrawPixel(SSD1306_t *oled, int16_t x, int16_t y, SSD1306_Color_t color)
{
  uint16_t index;
  uint8_t mask;

  if (!oled || x < 0 || y < 0 || x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT)
  {
    return;
  }

  /*
   * Один байт в буфере хранит 8 вертикальных пикселей.
   * y / 8 выбирает страницу, y & 7 выбирает бит.
   */
  index = (uint16_t)x + (uint16_t)(y / 8) * SSD1306_WIDTH;
  mask = (uint8_t)(1U << (y & 7));

  if (color == SSD1306_COLOR_WHITE)
  {
    oled->buffer[index] |= mask;
  }
  else if (color == SSD1306_COLOR_BLACK)
  {
    oled->buffer[index] &= (uint8_t)~mask;
  }
  else
  {
    oled->buffer[index] ^= mask;
  }
}

void SSD1306_DrawLine(SSD1306_t *oled, int16_t x0, int16_t y0, int16_t x1, int16_t y1, SSD1306_Color_t color)
{
  int16_t dx;
  int16_t sx;
  int16_t dy;
  int16_t sy;
  int16_t err;
  int16_t e2;

  if (!oled)
  {
    return;
  }

  /* Алгоритм Брезенхэма для линии. */
  dx = ssd1306_abs16((int16_t)(x1 - x0));
  sx = (x0 < x1) ? 1 : -1;
  dy = (int16_t)-ssd1306_abs16((int16_t)(y1 - y0));
  sy = (y0 < y1) ? 1 : -1;
  err = (int16_t)(dx + dy);

  while (1)
  {
    SSD1306_DrawPixel(oled, x0, y0, color);
    if (x0 == x1 && y0 == y1)
    {
      break;
    }

    e2 = (int16_t)(2 * err);
    if (e2 >= dy)
    {
      err = (int16_t)(err + dy);
      x0 = (int16_t)(x0 + sx);
    }
    if (e2 <= dx)
    {
      err = (int16_t)(err + dx);
      y0 = (int16_t)(y0 + sy);
    }
  }
}

void SSD1306_DrawRect(SSD1306_t *oled, int16_t x, int16_t y, int16_t width, int16_t height, SSD1306_Color_t color)
{
  if (!oled || width <= 0 || height <= 0)
  {
    return;
  }

  SSD1306_DrawLine(oled, x, y, (int16_t)(x + width - 1), y, color);
  SSD1306_DrawLine(oled, x, (int16_t)(y + height - 1), (int16_t)(x + width - 1), (int16_t)(y + height - 1), color);
  SSD1306_DrawLine(oled, x, y, x, (int16_t)(y + height - 1), color);
  SSD1306_DrawLine(oled, (int16_t)(x + width - 1), y, (int16_t)(x + width - 1), (int16_t)(y + height - 1), color);
}

void SSD1306_FillRect(SSD1306_t *oled, int16_t x, int16_t y, int16_t width, int16_t height, SSD1306_Color_t color)
{
  if (!oled || width <= 0 || height <= 0)
  {
    return;
  }

  for (int16_t iy = y; iy < (y + height); iy++)
  {
    for (int16_t ix = x; ix < (x + width); ix++)
    {
      SSD1306_DrawPixel(oled, ix, iy, color);
    }
  }
}

void SSD1306_DrawEllipse(SSD1306_t *oled, int16_t cx, int16_t cy, int16_t rx, int16_t ry, SSD1306_Color_t color)
{
  if (!oled || rx <= 0 || ry <= 0)
  {
    return;
  }

  for (int16_t y = (int16_t)-ry; y <= ry; y++)
  {
    for (int16_t x = (int16_t)-rx; x <= rx; x++)
    {
      int32_t left = ((int32_t)x * x * ry * ry) + ((int32_t)y * y * rx * rx);
      int32_t right = (int32_t)rx * rx * ry * ry;
      int32_t border = right / 8;

      if (left >= (right - border) && left <= (right + border))
      {
        SSD1306_DrawPixel(oled, (int16_t)(cx + x), (int16_t)(cy + y), color);
      }
    }
  }
}

void SSD1306_DrawBitmap(SSD1306_t *oled,
                        int16_t x,
                        int16_t y,
                        const uint8_t *bitmap,
                        uint16_t width,
                        uint16_t height,
                        SSD1306_Color_t color)
{
  if (!oled || !bitmap || width == 0 || height == 0)
  {
    return;
  }

  for (uint16_t iy = 0; iy < height; iy++)
  {
    for (uint16_t ix = 0; ix < width; ix++)
    {
      uint16_t byte_index = (uint16_t)(ix + (iy / 8) * width);
      uint8_t bit_mask = (uint8_t)(1U << (iy & 7));

      if ((bitmap[byte_index] & bit_mask) != 0)
      {
        SSD1306_DrawPixel(oled, (int16_t)(x + ix), (int16_t)(y + iy), color);
      }
    }
  }
}

static int16_t ssd1306_abs16(int16_t value)
{
  if (value < 0)
  {
    return (int16_t)-value;
  }

  return value;
}
