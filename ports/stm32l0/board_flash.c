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

#ifndef BUILD_NO_TINYUSB
#include "tusb.h"
#endif

#define FLASH_BASE_ADDR   0x08000000UL
#define PAGE_COUNT (BOARD_FLASH_SIZE / BOARD_PAGE_SIZE ) // 64KB / 128 bytes = 512 pages

static uint8_t erased_pages[PAGE_COUNT] = { 0 };

static bool is_blank(uint32_t addr, uint32_t size)
{
  for ( uint32_t i = 0; i < size; i += sizeof(uint32_t) )
  {
    if ( *(uint32_t*) (addr + i) != 0xffffffff )
    {
      return false;
    }
  }
  return true;
}

static bool flash_erase(uint32_t addr)
{
  // Calculate which page contains this address
  uint32_t page_addr = addr & ~(BOARD_PAGE_SIZE - 1);  // Align to page boundary
  uint32_t page_index = (page_addr - FLASH_BASE_ADDR) / BOARD_PAGE_SIZE;

  // Check if already erased
  if (erased_pages[page_index]) {
    return true;
  }

  // Check if page is blank
  if (!is_blank(page_addr, BOARD_PAGE_SIZE)) {

    FLASH_EraseInitTypeDef EraseInit = {};
    EraseInit.TypeErase = TYPEERASE_PAGES;
    EraseInit.PageAddress = page_addr;
    EraseInit.NbPages = 1;

    // erase the page
    uint32_t PageError = 0;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&EraseInit, &PageError);
    if (status != HAL_OK) {
      return false;
    }
    
    status = FLASH_WaitForLastOperation(HAL_MAX_DELAY);
    if (status != HAL_OK) {
      return false;
    }
  }

  erased_pages[page_index] = 1;
  return true;
}

static void flash_write(uint32_t dst, const uint8_t *src, int len)
{
  // Erase all pages that will be written to
  uint32_t start_page = dst & ~(BOARD_PAGE_SIZE - 1);
  uint32_t end_page = (dst + len - 1) & ~(BOARD_PAGE_SIZE - 1);
  
  for (uint32_t page = start_page; page <= end_page; page += BOARD_PAGE_SIZE) {
    flash_erase(page);
  }

  // Write in 32-bit words only, ensuring proper alignment
  for (int i = 0; i < len; i += 4)
  {
    uint32_t word;
    
    // Always write complete 32-bit words, padding with 0xFF if needed
    if (i + 4 <= len) {
      memcpy(&word, src + i, 4);
    } else {
      word = 0xFFFFFFFF;
      memcpy(&word, src + i, len - i);
    }

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, dst + i, word) != HAL_OK) {
      return; 
    }
    FLASH_WaitForLastOperation(HAL_MAX_DELAY);
  }
}

//--------------------------------------------------------------------+
// Board API
//--------------------------------------------------------------------+
void board_flash_init(void)
{

}

uint32_t board_flash_size(void)
{
  return BOARD_FLASH_SIZE;
}

void board_flash_read(uint32_t addr, void* buffer, uint32_t len)
{
  memcpy(buffer, (void*) addr, len);
}

void board_flash_flush(void)
{
}

bool board_flash_write(uint32_t addr, void const* data, uint32_t len)
{
  if (addr < BOARD_FLASH_APP_START) {
    return false; // Don't overwrite bootloader
  }
  
  HAL_StatusTypeDef status = HAL_FLASH_Unlock();
  if (status != HAL_OK) {
    return false;
  }
  
  flash_write(addr, data, len);
  
  HAL_FLASH_Lock();
  
  // Simple verification
  return (memcmp((void*)addr, data, len) == 0);
}

void board_flash_erase_app(void)
{
  // TODO implement later
}

#ifdef TINYUF2_SELF_UPDATE
/**
 * This will require enabling dual boot mode, making a backup and then copying
 */
void board_self_update(const uint8_t * bootloader_bin, uint32_t bootloader_len)
{
  (void) bootloader_bin;
  (void) bootloader_len;
}
#endif
