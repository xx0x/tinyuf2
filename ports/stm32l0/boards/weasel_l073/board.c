/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Vaclav Mach
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "board_api.h"
#include "stm32l0xx_hal.h"

static void Error_Handler(void)
{
  for (;;) {}
}

#define HAL_CHECK(x) if ((x) != HAL_OK) Error_Handler()

//--------------------------------------------------------------------+
// RCC Clock
//--------------------------------------------------------------------+

/* SYSCLK from HSI at 16MHz, USB from HSI48 trimmed by the CRS against the USB
 * SOF - crystal-less, the board has no external oscillator. */
void clock_init(void)
{
  // Set tick interrupt priority. The HAL default is deliberately out of range
  // for the 2 priority bits of a Cortex-M0+, which makes HAL_RCC_ClockConfig()
  // fail and leaves USB dead.
  HAL_InitTick((1UL << __NVIC_PRIO_BITS) - 1UL);

  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  RCC_CRSInitTypeDef RCC_CRSInitStruct = {0};

  // Configure the main internal regulator output voltage. PWR is off out of
  // reset, so its clock has to be running for the write to land.
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSE |
                                     RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSEState = RCC_LSE_OFF;      // leaves PC14/PC15 free for GPIO
  RCC_OscInitStruct.LSIState = RCC_LSI_OFF;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;   // USB clock
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

  HAL_CHECK(HAL_RCC_OscConfig(&RCC_OscInitStruct));

  // Initialize the CPU, AHB and APB bus clocks
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1; // 16MHz
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;  // 16MHz
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;  // 16MHz

  HAL_CHECK(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0));

  // Enable SYSCFG clock
  __HAL_RCC_SYSCFG_CLK_ENABLE();

  // Configure USB clock source to HSI48
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  HAL_CHECK(HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct));

  // Clock recovery system, disciplines HSI48 from the USB SOF
  __HAL_RCC_CRS_CLK_ENABLE();

  RCC_CRSInitStruct.Prescaler = RCC_CRS_SYNC_DIV1;
  RCC_CRSInitStruct.Source = RCC_CRS_SYNC_SOURCE_USB;
  RCC_CRSInitStruct.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000, 1000);
  RCC_CRSInitStruct.ErrorLimitValue = RCC_CRS_ERRORLIMIT_DEFAULT;
  RCC_CRSInitStruct.HSI48CalibrationValue = 0x20;
  HAL_RCCEx_CRSConfig(&RCC_CRSInitStruct);

  // Wait for HSI48 to be ready
  while ( __HAL_RCC_GET_FLAG(RCC_FLAG_HSI48RDY) == 0 ) {}
}
