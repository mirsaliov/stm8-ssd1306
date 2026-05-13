#include "ll_i2c.h"

#define LL_I2C_MAX_TIMEOUT 0xFFFFFFFFU

static uint32_t i2c_get_tick(void);
static void i2c_delay(volatile uint32_t cycles);
static uint32_t i2c_get_pclk1_hz(void);
static uint32_t i2c_get_hclk_hz(void);
static uint32_t i2c_prescaler(uint32_t value, const uint16_t *table);

static void i2c_gpio_clock(GPIO_TypeDef *port);
static void i2c_clock_on(I2C_TypeDef *I2Cx);
static void i2c_clock_off(I2C_TypeDef *I2Cx);
static void i2c_pin_init(const LL_I2C_PinConfig_t *pin, uint32_t pull, uint32_t speed);
static void i2c_pin_deinit(const LL_I2C_PinConfig_t *pin);
static void i2c_periph_init(LL_I2C_Handle_t *handle);
static void i2c_clear_addr(I2C_TypeDef *I2Cx);
static void i2c_stop(I2C_TypeDef *I2Cx);
static void i2c_save_error(LL_I2C_Handle_t *handle);
static LL_I2C_Status_t i2c_wait_flag(LL_I2C_Handle_t *handle, uint32_t flag, uint8_t state, uint32_t timeout_ms);
static LL_I2C_Status_t i2c_wait_free(LL_I2C_Handle_t *handle, uint32_t timeout_ms);

LL_I2C_Status_t LL_I2C_Init(LL_I2C_Handle_t *handle, const LL_I2C_Config_t *config)
{
  if (!handle || !config || !config->instance || !config->scl.port || !config->sda.port)
  {
    return LL_I2C_INVALID_PARAM;
  }

  handle->config = *config;
  handle->busy = 0;
  handle->last_status = LL_I2C_OK;
  handle->last_error = 0;

  /*
   * Включаем тактирование GPIO и I2C.
   * Пока тактирование выключено, регистры периферии менять нельзя.
   */
  i2c_gpio_clock(config->scl.port);
  i2c_gpio_clock(config->sda.port);
  i2c_clock_on(config->instance);

  /*
   * Настраиваем SCL и SDA как alternate function open-drain.
   * Для I2C это обязательный тип выхода.
   */
  i2c_pin_init(&config->scl, config->gpio_pull, config->gpio_speed);
  i2c_pin_init(&config->sda, config->gpio_pull, config->gpio_speed);

  i2c_periph_init(handle);
  return LL_I2C_OK;
}

LL_I2C_Status_t LL_I2C_DeInit(LL_I2C_Handle_t *handle)
{
  if (!handle)
  {
    return LL_I2C_INVALID_PARAM;
  }

  handle->config.instance->CR1 &= ~I2C_CR1_PE;
  i2c_pin_deinit(&handle->config.scl);
  i2c_pin_deinit(&handle->config.sda);
  i2c_clock_off(handle->config.instance);
  handle->busy = 0;
  handle->last_status = LL_I2C_OK;

  return LL_I2C_OK;
}

LL_I2C_Status_t LL_I2C_MasterTransmit(LL_I2C_Handle_t *handle,
                                      uint16_t device_address,
                                      const uint8_t *data,
                                      uint16_t size,
                                      uint32_t timeout_ms)
{
  I2C_TypeDef *I2Cx;
  LL_I2C_Status_t status;

  if (!handle || !data || size == 0)
  {
    return LL_I2C_INVALID_PARAM;
  }

  if (handle->busy)
  {
    return LL_I2C_BUSY;
  }

  handle->busy = 1;
  handle->last_status = LL_I2C_OK;
  I2Cx = handle->config.instance;

  status = i2c_wait_free(handle, timeout_ms);
  if (status != LL_I2C_OK) goto error;

  /* START - начало передачи. */
  I2Cx->CR1 |= I2C_CR1_START;
  status = i2c_wait_flag(handle, I2C_SR1_SB, 1, timeout_ms);
  if (status != LL_I2C_OK) goto error;

  /* Адрес уже сдвинут влево. Младший бит 0 - запись. */
  I2Cx->DR = (uint8_t)(device_address & 0xFE);
  status = i2c_wait_flag(handle, I2C_SR1_ADDR, 1, timeout_ms);
  if (status != LL_I2C_OK) goto error;
  i2c_clear_addr(I2Cx);

  for (uint16_t i = 0; i < size; i++)
  {
    /* TXE - регистр данных пуст, можно писать байт. */
    status = i2c_wait_flag(handle, I2C_SR1_TXE, 1, timeout_ms);
    if (status != LL_I2C_OK) goto error;
    I2Cx->DR = data[i];
  }

  /* BTF - последний байт полностью ушел на шину. */
  status = i2c_wait_flag(handle, I2C_SR1_BTF, 1, timeout_ms);
  if (status != LL_I2C_OK) goto error;

  i2c_stop(I2Cx);
  handle->busy = 0;
  handle->last_status = LL_I2C_OK;
  return LL_I2C_OK;

error:
  i2c_stop(I2Cx);
  handle->busy = 0;
  if (handle->config.auto_recovery)
  {
    (void)LL_I2C_Recover(handle);
  }
  return status;
}

LL_I2C_Status_t LL_I2C_MasterReceive(LL_I2C_Handle_t *handle,
                                     uint16_t device_address,
                                     uint8_t *data,
                                     uint16_t size,
                                     uint32_t timeout_ms)
{
  I2C_TypeDef *I2Cx;
  LL_I2C_Status_t status;

  if (!handle || !data || size == 0)
  {
    return LL_I2C_INVALID_PARAM;
  }

  if (handle->busy)
  {
    return LL_I2C_BUSY;
  }

  handle->busy = 1;
  handle->last_status = LL_I2C_OK;
  I2Cx = handle->config.instance;

  status = i2c_wait_free(handle, timeout_ms);
  if (status != LL_I2C_OK) goto error;

  if (size == 1)
  {
    I2Cx->CR1 &= ~I2C_CR1_ACK;
  }
  else
  {
    I2Cx->CR1 |= I2C_CR1_ACK;
  }

  I2Cx->CR1 |= I2C_CR1_START;
  status = i2c_wait_flag(handle, I2C_SR1_SB, 1, timeout_ms);
  if (status != LL_I2C_OK) goto error;

  /* Младший бит 1 - чтение. */
  I2Cx->DR = (uint8_t)(device_address | 0x01);
  status = i2c_wait_flag(handle, I2C_SR1_ADDR, 1, timeout_ms);
  if (status != LL_I2C_OK) goto error;

  if (size == 1)
  {
    i2c_clear_addr(I2Cx);
    I2Cx->CR1 |= I2C_CR1_STOP;
  }
  else
  {
    i2c_clear_addr(I2Cx);
  }

  for (uint16_t i = 0; i < size; i++)
  {
    if ((size > 1) && (i == (size - 2)))
    {
      I2Cx->CR1 &= ~I2C_CR1_ACK;
      I2Cx->CR1 |= I2C_CR1_STOP;
    }

    status = i2c_wait_flag(handle, I2C_SR1_RXNE, 1, timeout_ms);
    if (status != LL_I2C_OK) goto error;
    data[i] = (uint8_t)I2Cx->DR;
  }

  I2Cx->CR1 |= I2C_CR1_ACK;
  handle->busy = 0;
  handle->last_status = LL_I2C_OK;
  return LL_I2C_OK;

error:
  i2c_stop(I2Cx);
  I2Cx->CR1 |= I2C_CR1_ACK;
  handle->busy = 0;
  if (handle->config.auto_recovery)
  {
    (void)LL_I2C_Recover(handle);
  }
  return status;
}

uint8_t LL_I2C_IsBusy(const LL_I2C_Handle_t *handle)
{
  if (!handle)
  {
    return 0;
  }
  return handle->busy;
}

LL_I2C_Status_t LL_I2C_Recover(LL_I2C_Handle_t *handle)
{
  GPIO_TypeDef *scl_port;
  GPIO_TypeDef *sda_port;
  uint32_t scl_num;
  uint32_t sda_num;

  if (!handle)
  {
    return LL_I2C_INVALID_PARAM;
  }

  scl_port = handle->config.scl.port;
  sda_port = handle->config.sda.port;
  scl_num = (uint32_t)__builtin_ctz(handle->config.scl.pin);
  sda_num = (uint32_t)__builtin_ctz(handle->config.sda.pin);

  /*
   * Если ведомое устройство зависло и держит SDA,
   * 9 импульсов SCL помогают освободить шину.
   */
  handle->config.instance->CR1 &= ~I2C_CR1_PE;

  scl_port->MODER = (scl_port->MODER & ~(3UL << (scl_num * 2))) | (1UL << (scl_num * 2));
  sda_port->MODER = (sda_port->MODER & ~(3UL << (sda_num * 2))) | (1UL << (sda_num * 2));
  scl_port->OTYPER |= handle->config.scl.pin;
  sda_port->OTYPER |= handle->config.sda.pin;

  for (uint8_t i = 0; i < 9; i++)
  {
    scl_port->BSRR = (uint32_t)handle->config.scl.pin << 16;
    i2c_delay(3000);
    scl_port->BSRR = handle->config.scl.pin;
    i2c_delay(3000);
  }

  i2c_pin_init(&handle->config.scl, handle->config.gpio_pull, handle->config.gpio_speed);
  i2c_pin_init(&handle->config.sda, handle->config.gpio_pull, handle->config.gpio_speed);
  i2c_periph_init(handle);
  handle->busy = 0;
  handle->last_status = LL_I2C_OK;

  return LL_I2C_OK;
}

uint32_t LL_I2C_GetLastError(const LL_I2C_Handle_t *handle)
{
  if (!handle)
  {
    return 0;
  }
  return handle->last_error;
}

static uint32_t i2c_get_tick(void)
{
  extern volatile uint32_t uwTick;
  return uwTick;
}

static void i2c_delay(volatile uint32_t cycles)
{
  while (cycles-- > 0)
  {
    __NOP();
  }
}

static uint32_t i2c_get_pclk1_hz(void)
{
  static const uint16_t apb_table[8] = {1, 1, 1, 1, 2, 4, 8, 16};
  uint32_t ppre1 = (RCC->CFGR >> RCC_CFGR_PPRE1_Pos) & 0x07;
  return i2c_get_hclk_hz() / i2c_prescaler(ppre1, apb_table);
}

static uint32_t i2c_get_hclk_hz(void)
{
  static const uint16_t ahb_table[16] = {1, 1, 1, 1, 1, 1, 1, 1, 2, 4, 8, 16, 64, 128, 256, 512};
  uint32_t hpre = (RCC->CFGR >> RCC_CFGR_HPRE_Pos) & 0x0F;
  return SystemCoreClock / i2c_prescaler(hpre, ahb_table);
}

static uint32_t i2c_prescaler(uint32_t value, const uint16_t *table)
{
  return table[value];
}

static void i2c_gpio_clock(GPIO_TypeDef *port)
{
  if (port == GPIOA) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  else if (port == GPIOB) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  else if (port == GPIOC) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  else if (port == GPIOD) RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
  else if (port == GPIOE) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
  else if (port == GPIOF) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN;
  else if (port == GPIOG) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOGEN;
  else if (port == GPIOH) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN;
  else if (port == GPIOI) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOIEN;
}

static void i2c_clock_on(I2C_TypeDef *I2Cx)
{
  if (I2Cx == I2C1) RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
  else if (I2Cx == I2C2) RCC->APB1ENR |= RCC_APB1ENR_I2C2EN;
  else if (I2Cx == I2C3) RCC->APB1ENR |= RCC_APB1ENR_I2C3EN;
}

static void i2c_clock_off(I2C_TypeDef *I2Cx)
{
  if (I2Cx == I2C1) RCC->APB1ENR &= ~RCC_APB1ENR_I2C1EN;
  else if (I2Cx == I2C2) RCC->APB1ENR &= ~RCC_APB1ENR_I2C2EN;
  else if (I2Cx == I2C3) RCC->APB1ENR &= ~RCC_APB1ENR_I2C3EN;
}

static void i2c_pin_init(const LL_I2C_PinConfig_t *pin, uint32_t pull, uint32_t speed)
{
  uint32_t num = (uint32_t)__builtin_ctz(pin->pin);
  uint32_t afr = num / 8;
  uint32_t shift = (num % 8) * 4;

  pin->port->MODER &= ~(3UL << (num * 2));
  pin->port->MODER |=  (2UL << (num * 2));
  pin->port->OTYPER |= pin->pin;
  pin->port->OSPEEDR &= ~(3UL << (num * 2));
  pin->port->OSPEEDR |=  ((speed & 3U) << (num * 2));
  pin->port->PUPDR &= ~(3UL << (num * 2));
  pin->port->PUPDR |=  ((pull & 3U) << (num * 2));
  pin->port->AFR[afr] &= ~(0x0FUL << shift);
  pin->port->AFR[afr] |=  ((pin->alternate & 0x0F) << shift);
}

static void i2c_pin_deinit(const LL_I2C_PinConfig_t *pin)
{
  uint32_t num = (uint32_t)__builtin_ctz(pin->pin);
  pin->port->MODER &= ~(3UL << (num * 2));
  pin->port->PUPDR &= ~(3UL << (num * 2));
}

static void i2c_periph_init(LL_I2C_Handle_t *handle)
{
  I2C_TypeDef *I2Cx = handle->config.instance;
  uint32_t pclk1 = i2c_get_pclk1_hz();
  uint32_t pclk1_mhz = pclk1 / 1000000;
  uint32_t speed = handle->config.clock_speed;
  uint32_t ccr;

  if (speed == 0) speed = 100000;

  I2Cx->CR1 &= ~I2C_CR1_PE;
  I2Cx->CR1 |= I2C_CR1_SWRST;
  I2Cx->CR1 &= ~I2C_CR1_SWRST;
  I2Cx->CR2 = pclk1_mhz & I2C_CR2_FREQ;
  I2Cx->OAR1 = 0x4000U | handle->config.own_address1;

  if (speed <= 100000)
  {
    ccr = pclk1 / (speed * 2);
    if (ccr < 4) ccr = 4;
    I2Cx->CCR = ccr & I2C_CCR_CCR;
    I2Cx->TRISE = pclk1_mhz + 1;
  }
  else
  {
    ccr = pclk1 / (speed * 3);
    if (ccr == 0) ccr = 1;
    I2Cx->CCR = I2C_CCR_FS | (ccr & I2C_CCR_CCR);
    I2Cx->TRISE = ((pclk1_mhz * 300) / 1000) + 1;
  }

  I2Cx->CR1 |= I2C_CR1_ACK;
  I2Cx->CR1 |= I2C_CR1_PE;
}

static void i2c_clear_addr(I2C_TypeDef *I2Cx)
{
  volatile uint32_t tmp;
  tmp = I2Cx->SR1;
  tmp = I2Cx->SR2;
  (void)tmp;
}

static void i2c_stop(I2C_TypeDef *I2Cx)
{
  I2Cx->CR1 |= I2C_CR1_STOP;
}

static void i2c_save_error(LL_I2C_Handle_t *handle)
{
  I2C_TypeDef *I2Cx = handle->config.instance;
  handle->last_error = I2Cx->SR1 & (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR | I2C_SR1_TIMEOUT);
  I2Cx->SR1 &= ~(I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR | I2C_SR1_TIMEOUT);
}

static LL_I2C_Status_t i2c_wait_flag(LL_I2C_Handle_t *handle, uint32_t flag, uint8_t state, uint32_t timeout_ms)
{
  uint32_t start = i2c_get_tick();
  I2C_TypeDef *I2Cx = handle->config.instance;

  while ((((I2Cx->SR1 & flag) != 0) ? 1U : 0U) != state)
  {
    if ((I2Cx->SR1 & (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR | I2C_SR1_TIMEOUT)) != 0)
    {
      i2c_save_error(handle);
      handle->last_status = LL_I2C_ERROR;
      return LL_I2C_ERROR;
    }

    if ((timeout_ms != LL_I2C_MAX_TIMEOUT) && ((i2c_get_tick() - start) >= timeout_ms))
    {
      handle->last_status = LL_I2C_TIMEOUT;
      return LL_I2C_TIMEOUT;
    }
  }

  return LL_I2C_OK;
}

static LL_I2C_Status_t i2c_wait_free(LL_I2C_Handle_t *handle, uint32_t timeout_ms)
{
  uint32_t start = i2c_get_tick();

  while ((handle->config.instance->SR2 & I2C_SR2_BUSY) != 0)
  {
    if ((timeout_ms != LL_I2C_MAX_TIMEOUT) && ((i2c_get_tick() - start) >= timeout_ms))
    {
      handle->last_status = LL_I2C_TIMEOUT;
      return LL_I2C_TIMEOUT;
    }
  }

  return LL_I2C_OK;
}
