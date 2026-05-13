#ifndef LL_I2C_H
#define LL_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx.h"
#include <stdint.h>

/*
 * Нижний уровень I2C.
 *
 * Этот модуль сам настраивает GPIO и регистры I2C.
 * HAL_I2C_Init и HAL_GPIO_Init здесь не используются.
 *
 * В этой версии оставлен только блокирующий режим:
 * функция запускает передачу и ждет, пока она закончится.
 */

typedef enum
{
  LL_I2C_OK = 0,
  LL_I2C_ERROR,
  LL_I2C_BUSY,
  LL_I2C_TIMEOUT,
  LL_I2C_INVALID_PARAM
} LL_I2C_Status_t;

typedef struct
{
  GPIO_TypeDef *port;   /* GPIOA, GPIOB и т.д. */
  uint16_t pin;         /* Маска пина, например (1U << 6) для PB6. */
  uint32_t alternate;   /* Номер альтернативной функции. Для I2C1 это AF4. */
} LL_I2C_PinConfig_t;

typedef struct
{
  I2C_TypeDef *instance;      /* I2C1, I2C2 или I2C3. */
  LL_I2C_PinConfig_t scl;     /* Линия тактирования. */
  LL_I2C_PinConfig_t sda;     /* Линия данных. */
  uint32_t clock_speed;       /* Частота I2C: 100000 или 400000 Гц. */
  uint32_t own_address1;       /* Собственный адрес. Для master обычно 0. */
  uint32_t gpio_pull;         /* 0 - нет подтяжки, 1 - pull-up, 2 - pull-down. */
  uint32_t gpio_speed;        /* Скорость GPIO: 0..3. Обычно 3. */
  uint8_t auto_recovery;      /* 1 - пробовать восстановить шину при ошибке. */
} LL_I2C_Config_t;

typedef struct
{
  LL_I2C_Config_t config;
  volatile uint8_t busy;
  volatile LL_I2C_Status_t last_status;
  volatile uint32_t last_error;
} LL_I2C_Handle_t;

LL_I2C_Status_t LL_I2C_Init(LL_I2C_Handle_t *handle, const LL_I2C_Config_t *config);
LL_I2C_Status_t LL_I2C_DeInit(LL_I2C_Handle_t *handle);

LL_I2C_Status_t LL_I2C_MasterTransmit(LL_I2C_Handle_t *handle,
                                      uint16_t device_address,
                                      const uint8_t *data,
                                      uint16_t size,
                                      uint32_t timeout_ms);

LL_I2C_Status_t LL_I2C_MasterReceive(LL_I2C_Handle_t *handle,
                                     uint16_t device_address,
                                     uint8_t *data,
                                     uint16_t size,
                                     uint32_t timeout_ms);

uint8_t LL_I2C_IsBusy(const LL_I2C_Handle_t *handle);
LL_I2C_Status_t LL_I2C_Recover(LL_I2C_Handle_t *handle);
uint32_t LL_I2C_GetLastError(const LL_I2C_Handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif
