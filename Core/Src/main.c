/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN Includes */
#include "ssd1306.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
/*
 * Объект дисплея сделан так же просто, как в примере EEPROM:
 * в структуре указываем I2C, адрес, скорость и флаги действий.
 */
SSD1306_t oled = {
    .I2Cx = I2C1,
    .dev_adr = SSD1306_DEFAULT_ADDR,
    .clock_speed = 400000,
    .i2c_user_init = 1,
    .display_init = 0,
    .update = 0,
    .clear = 0,
    .clear_color = SSD1306_COLOR_BLACK,
    .timeout_ms = 100,
    .last_error = 0
};
/* USER CODE END PV */

void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
static void APP_DrawDemoScreen(SSD1306_t *oled);
/* USER CODE END PFP */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  /* 1. Настраиваем I2C и пины PB6/PB7. */
  SSD1306(&oled);

  /* 2. Инициализируем контроллер SSD1306. */
  oled.display_init = 1;
  SSD1306(&oled);

  /* 3. Рисуем тестовую картинку в буфер. */
  APP_DrawDemoScreen(&oled);

  /* 4. Отправляем буфер на экран. */
  oled.update = 1;
  SSD1306(&oled);

  while (1)
  {
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 |
                                RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
static void APP_DrawDemoScreen(SSD1306_t *oled)
{
  /* Маленькая картинка 16x16 для проверки вывода bitmap. */
  static const uint8_t icon_16x16[] =
  {
    0xF0, 0x08, 0x04, 0xE2, 0x12, 0x0A, 0x0A, 0x12,
    0xE2, 0x04, 0x08, 0xF0, 0x00, 0x00, 0x00, 0x00,
    0x0F, 0x10, 0x20, 0x47, 0x48, 0x50, 0x50, 0x48,
    0x47, 0x20, 0x10, 0x0F, 0x00, 0x00, 0x00, 0x00
  };

  SSD1306_Clear(oled, SSD1306_COLOR_BLACK);
  SSD1306_DrawRect(oled, 0, 0, SSD1306_WIDTH, SSD1306_HEIGHT, SSD1306_COLOR_WHITE);
  SSD1306_DrawLine(oled, 4, 12, 123, 12, SSD1306_COLOR_WHITE);
  SSD1306_DrawLine(oled, 4, 52, 123, 52, SSD1306_COLOR_WHITE);
  SSD1306_DrawEllipse(oled, 96, 32, 20, 12, SSD1306_COLOR_WHITE);
  SSD1306_FillRect(oled, 12, 24, 34, 18, SSD1306_COLOR_WHITE);
  SSD1306_DrawRect(oled, 14, 26, 30, 14, SSD1306_COLOR_BLACK);
  SSD1306_DrawBitmap(oled, 56, 24, icon_16x16, 16, 16, SSD1306_COLOR_WHITE);
}
/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif
