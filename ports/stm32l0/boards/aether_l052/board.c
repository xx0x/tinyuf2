#include "board_api.h"
#include "stm32l0xx_hal.h"

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
    // Without this, USB does not function.
    HAL_InitTick((1UL << __NVIC_PRIO_BITS) - 1UL);

    RCC_OscInitTypeDef RCC_OscInitStruct = {};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {};

    // Configure the main internal regulator output voltage
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    // Initialize the RCC Oscillators according to the specified parameters
    // Use HSI (16MHz) and HSI48 (48MHz) for USB
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSE | RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_HSI48;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.LSEState = RCC_LSE_OFF;      // Disable LSE for power saving
    RCC_OscInitStruct.LSIState = RCC_LSI_OFF;      // Disable LSI for power saving
    RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;   // Enable HSI48 for USB
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE; // Don't use PLL for simplicity

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

    // Configure the clock recovery system (CRS) for crystal-less USB
    __HAL_RCC_CRS_CLK_ENABLE();
    
    RCC_CRSInitTypeDef RCC_CRSInitStruct = {0};
    RCC_CRSInitStruct.Prescaler = RCC_CRS_SYNC_DIV1;
    RCC_CRSInitStruct.Source = RCC_CRS_SYNC_SOURCE_USB;
    RCC_CRSInitStruct.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000, 1000);
    RCC_CRSInitStruct.ErrorLimitValue = RCC_CRS_ERRORLIMIT_DEFAULT;
    RCC_CRSInitStruct.HSI48CalibrationValue = 0x20;
    HAL_RCCEx_CRSConfig(&RCC_CRSInitStruct);

    // Wait for HSI48 to be ready
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_HSI48RDY) == 0) {}

}