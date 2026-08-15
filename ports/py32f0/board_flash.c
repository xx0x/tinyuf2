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

// The PY32F07x flash controller can only program a full page (64 words) at a
// time and the target address must be page aligned, so partial writes are
// staged through this buffer.
static uint32_t page_buf[BOARD_PAGE_SIZE / 4];

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

// Erase + program a single page. data must hold BOARD_PAGE_SIZE bytes and be
// 4-byte aligned, page_addr must be page aligned.
static bool flash_write_page(uint32_t page_addr, uint32_t const* data)
{
  // Nothing to do if flash already holds the requested content
  if ( memcmp((void*) page_addr, data, BOARD_PAGE_SIZE) == 0 )
  {
    return true;
  }

  if ( !is_blank(page_addr, BOARD_PAGE_SIZE) )
  {
    FLASH_EraseInitTypeDef EraseInit = { 0 };
    EraseInit.TypeErase = FLASH_TYPEERASE_PAGEERASE;
    EraseInit.PageAddress = page_addr;
    EraseInit.NbPages = 1;

    uint32_t PageError = 0;
    if ( HAL_FLASH_Erase(&EraseInit, &PageError) != HAL_OK )
    {
      return false;
    }
  }

  // HAL_FLASH_Program() takes a non-const pointer but only reads from it
  if ( HAL_FLASH_Program(FLASH_TYPEPROGRAM_PAGE, page_addr, (uint32_t*) (uintptr_t) data) != HAL_OK )
  {
    return false;
  }

  // Verify contents
  if ( memcmp((void*) page_addr, data, BOARD_PAGE_SIZE) != 0 )
  {
    TUF2_LOG1("Failed to write\r\n");
    return false;
  }

  return true;
}

//--------------------------------------------------------------------+
// Board API
//--------------------------------------------------------------------+
void board_flash_init(void)
{

}

uint32_t board_flash_size(void)
{
  // ghostfat maps CURRENT.UF2 starting at BOARD_FLASH_APP_START, so report only
  // the application region. Reporting the full device size would make the host
  // read past the end of flash while listing the drive.
  return BOARD_FLASH_SIZE - (BOARD_FLASH_APP_START - BOARD_FLASH_ADDR_ZERO);
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
  if ( HAL_FLASH_Unlock() != HAL_OK )
  {
    return false;
  }

  uint8_t const* src = (uint8_t const*) data;
  bool ok = true;

  while ( len && ok )
  {
    uint32_t const page_addr = addr & ~((uint32_t) BOARD_PAGE_SIZE - 1);
    uint32_t const offset = addr - page_addr;
    uint32_t chunk = BOARD_PAGE_SIZE - offset;
    if ( chunk > len ) chunk = len;

    if ( chunk == BOARD_PAGE_SIZE && ((uintptr_t) src & 3) == 0 )
    {
      // Full page write straight from the caller's buffer
      ok = flash_write_page(page_addr, (uint32_t const*) (uintptr_t) src);
    }
    else
    {
      // Partial (or misaligned) write: merge with the current page content
      memcpy(page_buf, (void*) page_addr, BOARD_PAGE_SIZE);
      memcpy((uint8_t*) page_buf + offset, src, chunk);
      ok = flash_write_page(page_addr, page_buf);
    }

    addr += chunk;
    src += chunk;
    len -= chunk;
  }

  HAL_FLASH_Lock();
  return ok;
}

void board_flash_erase_app(void)
{
  // TODO implement later
}

#ifdef TINYUF2_SELF_UPDATE
void board_self_update(const uint8_t * bootloader_bin, uint32_t bootloader_len)
{
  (void) bootloader_bin;
  (void) bootloader_len;
}
#endif
