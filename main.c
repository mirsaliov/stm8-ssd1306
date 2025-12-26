#include <iostm8s207.h>
#include  <oled_ssd1306.h>

void main(void)
{
    /* I2C pins: PE1=SCL, PE2=SDA */
    PE_DDR &= ~0x06;
    PE_CR1 |=  0x06;
    PE_CR2 |=  0x06;

    CLK_PCKENR1 |= 0x20;

    I2C_CR1 &= ~0x01;
    I2C_FREQR  = 0x10;
    I2C_CCRL   = 0x50;
    I2C_CCRH   = 0x00;
    I2C_TRISER = 0x11;
    I2C_CR1 |= 0x01;

    while (I2C_SR3 & 0x02);

    oled_init();
    oled_clear();

    oled_setpos(0, 0);
    oled_puts("DANIIL GOODMAN\n");
    oled_puts("ANTON TOZHE");

    while (1);
}
