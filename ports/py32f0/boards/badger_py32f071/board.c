#include "board_api.h"
#include "py32f0xx_hal.h"

void Error_Handler(void) {
    for (;;) {

    }
}

#define HAL_CHECK(x) if (x != HAL_OK) Error_Handler()

//--------------------------------------------------------------------+
// RCC Clock
//--------------------------------------------------------------------+
void clock_init(void)
{
    // Set tick interrupt priority, default HAL value is intentionally invalid
    // and would make HAL_RCC_ClockConfig() fail.
    HAL_InitTick((1UL << __NVIC_PRIO_BITS) - 1UL);

    RCC_OscInitTypeDef RCC_OscInitStruct = {};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {};

    // The PY32F07x USB device is clocked from the system clock, which must be
    // exactly 48MHz. HSI trimmed to 24MHz through the PLL (x2) gets us there
    // without an external crystal, this is what Puya's own USB examples do.
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSE |
                                       RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_LSE;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_24MHz;
    RCC_OscInitStruct.HSEState = RCC_HSE_OFF;   // No crystal on the board
    RCC_OscInitStruct.LSIState = RCC_LSI_OFF;
    RCC_OscInitStruct.LSEState = RCC_LSE_OFF;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL2;

    HAL_CHECK(HAL_RCC_OscConfig(&RCC_OscInitStruct));

    // Initialize the CPU, AHB and APB bus clocks
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1; // 48MHz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;  // 48MHz

    HAL_CHECK(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1));
}
