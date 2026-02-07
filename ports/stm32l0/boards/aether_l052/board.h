/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Vaclav Mach
 * Copyright (c) 2018 Matthew McGowan for Blues Inc.
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

#ifndef BOARD_H_
#define BOARD_H_


// LED

#define LED_PORT              GPIOA
#define LED_PIN               GPIO_PIN_15
#define LED_STATE_ON          1

// FLASH

// Flash size of the board (64kB - 16kB bootloader = 48 kB)
#define BOARD_FLASH_SIZE  (48 * 1024)

// USB

#define USB_VID           0xf055
#define USB_PID           0xae52
#define USB_MANUFACTURER  "Generic"
#define USB_PRODUCT       "Aether L052"

// UF2

#define UF2_PRODUCT_NAME  USB_PRODUCT
#define UF2_BOARD_ID      "STM32L052-Aether_L052-v1"
#define UF2_VOLUME_LABEL  "AETHERBOOT"
#define UF2_INDEX_URL     "https://github.com/xx0x/aether-l052"

// UART is not available (won't fit into the 16 kB bootloader)

#define BOARD_STACK_APP_START (0x20000000U)
#define BOARD_STACK_APP_END   (0x20000000U + 8*1024)

// Flash layout: 16KB bootloader + 48KB app
#define BOARD_FLASH_APP_START (0x08004000U)

void clock_init(void);

#endif
