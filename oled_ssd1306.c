#include <iostm8s207.h>
#include "oled_ssd1306.h"
#include "ascii_font.h"

/*
 * Текущая позиция текстового курсора.
 * Используется ТОЛЬКО для текстового режима.
 */
static unsigned char cur_page = 0;
static unsigned char cur_col  = 0;
/* Формирование условия START на шине I2C */
static void i2c_start(void)
{
    I2C_CR2 |= 0x01;
    while (!(I2C_SR1 & 0x01));   // ожидание установки флага SB
}

/* Формирование условия STOP */
static void i2c_stop(void)
{
    I2C_CR2 |= 0x02;
}

/*
 * Передача адреса SSD1306.
 * Используется 7-битный адрес 0x3C,
 * в I2C передаётся как 0x78.
 */
static void i2c_addr(void)
{
    I2C_DR = 0x78;
    while (!(I2C_SR1 & 0x02));   // ожидание ACK
    (void)I2C_SR3;               // сброс флага ADDR
}

/* Передача одного байта данных */
static void i2c_tx(unsigned char d)
{
    I2C_DR = d;
    while (!(I2C_SR1 & 0x80));   // ожидание TXE
}
/*
 * Отправка одной команды дисплею SSD1306.
 * Команды управляют режимами и настройками.
 */
static void oled_cmd(unsigned char c)
{
    i2c_start();
    i2c_addr();
    i2c_tx(0x00);    // control byte: команда
    i2c_tx(c);
    i2c_stop();
}

/*
 * Начало передачи данных в видеопамять дисплея.
 * Все последующие байты трактуются как пиксели.
 */
static void oled_data_begin(void)
{
    i2c_start();
    i2c_addr();
    i2c_tx(0x40);    // control byte: данные
}

/* Завершение передачи данных */
static void oled_data_end(void)
{
    i2c_stop();
}
/*
 * Установка позиции курсора.
 * Обновляет как аппаратную позицию дисплея,
 * так и программные координаты курсора.
 */
void oled_setpos(unsigned char page, unsigned char col)
{
    cur_page = page;
    cur_col  = col;

    oled_cmd(0xB0 | (page & 7));     // выбор страницы
    oled_cmd(0x00 | (col & 0x0F));   // младшие биты столбца
    oled_cmd(0x10 | (col >> 4));     // старшие биты столбца
}

/*
 * Очистка всего экрана.
 * Последовательно очищаются все страницы дисплея.
 */
void oled_clear(void)
{
    unsigned char p;
    unsigned int i;

    for (p = 0; p < 8; p++)
    {
        oled_setpos(p, 0);
        oled_data_begin();
        for (i = 0; i < 128; i++)
            i2c_tx(0x00);    // выключение пикселей
        oled_data_end();
    }
}
/*
 * Инициализация SSD1306 в режиме 128x64.
 * Используется постраничная адресация.
 */
void oled_init(void)
{
    unsigned int d;
    for (d = 0; d < 20000; d++);   // задержка после подачи питания

    oled_cmd(0xAE);   // дисплей выключен
    oled_cmd(0xD5); oled_cmd(0x80);
    oled_cmd(0xA8); oled_cmd(0x3F);
    oled_cmd(0xD3); oled_cmd(0x00);
    oled_cmd(0x40);
    oled_cmd(0x8D); oled_cmd(0x14);   // включение charge pump
    oled_cmd(0x20); oled_cmd(0x02);   // постраничная адресация
    oled_cmd(0xA1);
    oled_cmd(0xC8);
    oled_cmd(0xDA); oled_cmd(0x12);
    oled_cmd(0x81); oled_cmd(0x7F);   // установка контраста
    oled_cmd(0xD9); oled_cmd(0xF1);
    oled_cmd(0xDB); oled_cmd(0x40);
    oled_cmd(0xA6);   // нормальный режим
    oled_cmd(0xAF);   // дисплей включен
}
/*
 * Вывод одного символа ASCII.
 * Поддерживается автоматический перенос строки
 * и переход на следующую страницу.
 */
void oled_putc(char c)
{
    unsigned char i;
    unsigned char idx;

    /* перевод строки */
    if (c == '\n')
    {
        cur_col = 0;
        cur_page++;
        if (cur_page >= 8) cur_page = 0;
        oled_setpos(cur_page, cur_col);
        return;
    }

    /* автоматический переход по ширине */
    if (cur_col > 122)
    {
        cur_col = 0;
        cur_page++;
        if (cur_page >= 8) cur_page = 0;
        oled_setpos(cur_page, cur_col);
    }

    /* защита от некорректных ASCII-кодов */
    if (c < 0x20 || c > 0x7E)
        c = ' ';

    idx = c - 0x20;

    oled_data_begin();
    for (i = 0; i < 5; i++)
        i2c_tx(ascii_font_5x7[idx][i]);
    i2c_tx(0x00);   // промежуток между символами
    oled_data_end();

    cur_col += 6;
}

/*
 * Вывод строки символов.
 * Символ '\n' обрабатывается как перевод строки.
 */
void oled_puts(char *s)
{
    while (*s)
    {
        oled_putc(*s);
        s++;
    }
}
