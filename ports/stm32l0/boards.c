/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Ha Thach for Adafruit Industries
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
#include "stm32l0xx.h"
#include "stm32l0xx_hal_conf.h"

#ifndef BUILD_NO_TINYUSB
#include "tusb.h"
#endif

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

#define STM32_UUID ((volatile uint32_t *) UID_BASE)

#if defined(BUTTON_PIN) && defined(BUTTON_PORT) && defined(BUTTON_STATE_ACTIVE)
  #define HAS_BUTTON 1
#else
  #define HAS_BUTTON 0
#endif

#if defined(LED_PIN) && defined(LED_PORT)
  #define HAS_LED 1
#else
  #define HAS_LED 0
#endif

#if HAS_BUTTON
/* Busy-wait using SysTick in polled mode (interrupt left disabled).
 *
 * Only called while the indicator timer is stopped, so it never races with
 * board_timer_start()/SysTick_Handler().
 */
static void delay_ms(uint32_t ms)
{
  SysTick->CTRL = 0;
  SysTick->LOAD = (SystemCoreClock / 1000U) - 1U;
  SysTick->VAL  = 0;
  SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

  while ( ms )
  {
    if ( SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk ) ms--;
  }

  SysTick->CTRL = 0;
}
#endif

void board_init(void)
{
  clock_init();
  SystemCoreClockUpdate();

  // disable systick
  board_timer_stop();

  // GPIOA carries the USB pins, the LED/button may sit on any of A/B/C
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {0};

#if HAS_BUTTON
  GPIO_InitStruct.Pin = BUTTON_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = (BUTTON_STATE_ACTIVE == 0) ? GPIO_PULLUP : GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUTTON_PORT, &GPIO_InitStruct);
#endif

#if HAS_BUTTON
  // Let the internal pull settle before board_app_valid2() samples the pin,
  // otherwise a floating input can be read as "button pressed".
  delay_ms(1);
#endif
}

void board_dfu_init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // USB Pins Init (PA11 = DM, PA12 = DP)
  GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Alternate = GPIO_AF2_USB;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // Enable USB clock
  __HAL_RCC_USB_CLK_ENABLE();

#if HAS_LED
  GPIO_InitStruct.Pin = LED_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);
  board_led_write(0);
#endif
}

void board_reset(void)
{
  NVIC_SystemReset();
}

void board_dfu_complete(void)
{
  /* Note: this is called from tud_msc_write10_complete_cb(), i.e. from inside
   * tud_task() and before the MSC driver has re-armed the bulk OUT endpoint for
   * the next command. Stalling here to give the host a grace period would only
   * NAK whatever it sends next, so the reset is immediate - as on every other
   * port. If the OS still had filesystem metadata queued when the last UF2
   * block landed, it will report a write error; that is inherent to how tinyuf2
   * finishes, not a sign the image is bad. */
  NVIC_SystemReset();
}

uint32_t board_button_read(void)
{
#if HAS_BUTTON
  return BUTTON_STATE_ACTIVE == HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN);
#else
  return 0;
#endif
}

bool board_app_valid(void)
{
  // Load vector table
  volatile uint32_t const * app_vector = (volatile uint32_t const*) BOARD_FLASH_APP_START;
  uint32_t sp = app_vector[0];
  uint32_t app_entry = app_vector[1];

  TUF2_LOG1_HEX(sp);
  TUF2_LOG1_HEX(app_entry);

  // 1st word is stack pointer (must be in SRAM region)
  if (sp < BOARD_STACK_APP_START || sp > BOARD_STACK_APP_END) return false;

  // 2nd word is App entry point (reset)
  if (app_entry < BOARD_FLASH_APP_START || app_entry > BOARD_FLASH_ADDR_ZERO + BOARD_FLASH_SIZE) {
    return false;
  }

  return true;
}

// Holding the button down at reset forces DFU mode regardless of app validity.
bool board_app_valid2(void)
{
  return !board_button_read();
}

void board_app_jump(void)
{
  volatile uint32_t const * app_vector = (volatile uint32_t const*) BOARD_FLASH_APP_START;
  uint32_t sp = app_vector[0];
  uint32_t app_entry = app_vector[1];

#if HAS_BUTTON
  HAL_GPIO_DeInit(BUTTON_PORT, BUTTON_PIN);
#endif

#if HAS_LED
  HAL_GPIO_DeInit(LED_PORT, LED_PIN);
#endif

  __HAL_RCC_GPIOA_CLK_DISABLE();
  __HAL_RCC_GPIOB_CLK_DISABLE();
  __HAL_RCC_GPIOC_CLK_DISABLE();

  HAL_RCC_DeInit();

  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;

  // Disable all Interrupts
  RCC->CIER = 0x00000000U;

  // Switch exception handlers to the application
  SCB->VTOR = (uint32_t) BOARD_FLASH_APP_START;

  // Set stack pointer
  __set_MSP(sp);

  // Jump to Application Entry
  asm("bx %0" ::"r"(app_entry));
}

uint8_t board_usb_get_serial(uint8_t serial_id[16])
{
  uint8_t const len = 12;
  uint32_t* serial_id32 = (uint32_t*) (uintptr_t) serial_id;

  serial_id32[0] = STM32_UUID[0];
  serial_id32[1] = STM32_UUID[1];
  serial_id32[2] = STM32_UUID[2];

  return len;
}

//--------------------------------------------------------------------+
// LED pattern
//--------------------------------------------------------------------+

void board_led_write(uint32_t state)
{
#if HAS_LED
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, state ? LED_STATE_ON : (1 - LED_STATE_ON));
#else
  (void) state;
#endif
}

void board_rgb_write(uint8_t const rgb[])
{
  (void) rgb;
}

//--------------------------------------------------------------------+
// Timer
//--------------------------------------------------------------------+

void board_timer_start(uint32_t ms)
{
  SysTick_Config( (SystemCoreClock/1000) * ms );
}

void board_timer_stop(void)
{
  SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
}

void SysTick_Handler(void)
{
  board_timer_handler();
}

// Board UART is disabled, the bootloader does not fit in 16KB with it enabled
int board_uart_write(void const * buf, int len)
{
  (void) buf; (void) len;
  return 0;
}

#ifndef BUILD_NO_TINYUSB
// Forward USB interrupt events to TinyUSB IRQ Handler
void USB_IRQHandler(void)
{
  tud_int_handler(0);
}
#endif

// Required by __libc_init_array in startup code if we are compiling using
// -nostdlib/-nostartfiles.
__attribute__((used)) void _init(void)
{

}
