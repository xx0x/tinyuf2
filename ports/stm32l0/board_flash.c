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

/* STM32L0 NVM driver.
 *
 * The NVM needs one ~3.9 ms high voltage cycle per operation, and that cycle
 * costs the same whether it programs a single word or a full 64-byte half page.
 * Programming word by word therefore wastes 15/16 of the available bandwidth -
 * a 100KB image takes about 100 s that way versus about 6 s with half pages,
 * which is slow enough to push USB MSC write commands towards the host's
 * timeout. So everything here is done in half pages.
 *
 * Neither NVM operation runs in place. Half page programming cannot: the array
 * is unreadable while the 16 words are being loaded. Page erase in principle
 * can - the CPU is simply stalled - but the 128KB and 192KB L07x/L08x parts are
 * dual bank and the bootloader sits in the same bank as most of the application,
 * so an erase there would be a fetch on the bank under operation. Both therefore
 * live in the .RamFunc section with interrupts masked, since every vector and
 * handler is in flash too.
 *
 * The registers are driven directly rather than through stm32l0xx_hal_flash*.c
 * because the HAL's wait loops are built on HAL_GetTick(), and this bootloader
 * keeps SysTick free for the LED indicator - a HAL timeout would never expire
 * and a wedged NVM would hang the bootloader instead of failing the write.
 */

#include "board_api.h"

#ifndef BUILD_NO_TINYUSB
#include "tusb.h"
#endif

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

#define FLASH_ERROR_MASK  (FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | \
                           FLASH_SR_OPTVERR | FLASH_SR_RDERR | FLASH_SR_FWWERR | \
                           FLASH_SR_NOTZEROERR)

// Bounded spin for the ~3.9 ms high voltage cycle. At the 16 MHz this port runs
// the loop body costs a handful of cycles, so this is an upper bound of a few
// hundred ms - deliberately loose, its only job is to turn a wedged NVM into a
// failed write rather than a hung bootloader.
#define FLASH_BSY_TIMEOUT (1000000UL)

#define FLASH_APP_END     (BOARD_FLASH_ADDR_ZERO + BOARD_FLASH_SIZE)

//--------------------------------------------------------------------+
// Low level NVM access
//--------------------------------------------------------------------+

static bool is_blank(uint32_t addr, uint32_t size)
{
  for ( uint32_t i = 0; i < size; i += sizeof(uint32_t) )
  {
    if ( *(volatile uint32_t*) (addr + i) != 0xffffffffUL ) return false;
  }
  return true;
}

static bool flash_unlock(void)
{
  if ( FLASH->PECR & FLASH_PECR_PELOCK )
  {
    FLASH->PEKEYR = FLASH_PEKEY1;
    FLASH->PEKEYR = FLASH_PEKEY2;
  }

  if ( FLASH->PECR & FLASH_PECR_PRGLOCK )
  {
    FLASH->PRGKEYR = FLASH_PRGKEY1;
    FLASH->PRGKEYR = FLASH_PRGKEY2;
  }

  return (FLASH->PECR & (FLASH_PECR_PELOCK | FLASH_PECR_PRGLOCK)) == 0;
}

static void flash_lock(void)
{
  // PELOCK also re-arms PRGLOCK and OPTLOCK
  FLASH->PECR |= FLASH_PECR_PELOCK;
}

/* Program one 64-byte half page.
 *
 * Runs from RAM: the flash array is inaccessible while the 16 words are being
 * loaded, so neither an instruction fetch nor a literal pool read may happen in
 * between - which is also why interrupts are masked. Everything the loop needs
 * is hoisted into locals before the FPRG window opens.
 *
 * @return the FLASH->SR value latched after the operation, or the full error
 *         mask if the NVM never went idle.
 */
static __attribute__((section(".RamFunc"), noinline, used))
uint32_t flash_program_half_page(uint32_t addr, uint32_t const* src)
{
  volatile uint32_t* const dst  = (volatile uint32_t*) addr;
  volatile uint32_t* const pecr = &FLASH->PECR;
  volatile uint32_t* const sr   = &FLASH->SR;
  uint32_t words = BOARD_HALF_PAGE_SIZE / sizeof(uint32_t);
  uint32_t timeout = FLASH_BSY_TIMEOUT;
  uint32_t status;

  __disable_irq();

  *pecr |= (FLASH_PECR_FPRG | FLASH_PECR_PROG);

  // 16 back to back word writes to the same address, the NVM walks the half
  // page internally
  while ( words-- ) *dst = *src++;

  while ( (*sr & FLASH_SR_BSY) && timeout ) timeout--;
  status = *sr;

  *pecr &= ~(FLASH_PECR_FPRG | FLASH_PECR_PROG);

  __enable_irq();

  return timeout ? status : FLASH_ERROR_MASK;
}

/* Erase one 128-byte page. Runs from RAM for the dual bank reason above. */
static __attribute__((section(".RamFunc"), noinline, used))
uint32_t flash_erase_page_ram(uint32_t addr)
{
  volatile uint32_t* const dst  = (volatile uint32_t*) addr;
  volatile uint32_t* const pecr = &FLASH->PECR;
  volatile uint32_t* const sr   = &FLASH->SR;
  uint32_t timeout = FLASH_BSY_TIMEOUT;
  uint32_t status;

  __disable_irq();

  *pecr |= (FLASH_PECR_ERASE | FLASH_PECR_PROG);

  // erasing is triggered by writing zero anywhere in the page
  *dst = 0x00000000UL;

  while ( (*sr & FLASH_SR_BSY) && timeout ) timeout--;
  status = *sr;

  *pecr &= ~(FLASH_PECR_ERASE | FLASH_PECR_PROG);

  __enable_irq();

  return timeout ? status : FLASH_ERROR_MASK;
}

static bool flash_erase_page(uint32_t addr)
{
  FLASH->SR = FLASH_ERROR_MASK;   // rc_w1

  if ( flash_erase_page_ram(addr) & FLASH_ERROR_MASK )
  {
    FLASH->SR = FLASH_ERROR_MASK;
    return false;
  }

  return true;
}

/* Update the [offset, offset+count) slice of a 128-byte page. */
static bool flash_write_page(uint32_t page, uint32_t offset, uint8_t const* src, uint32_t count)
{
  uint32_t buf[BOARD_PAGE_SIZE / sizeof(uint32_t)];

  memcpy(buf, (void const*) page, BOARD_PAGE_SIZE);
  memcpy((uint8_t*) buf + offset, src, count);

  /* Skip pages that already hold what we want. This makes re-flashing an
   * unchanged image nearly free, and it is also what makes a repeated write of
   * the same block - the host retrying, or the OS rewriting a sector - safe
   * without tracking which pages have already been erased. */
  if ( memcmp(buf, (void const*) page, BOARD_PAGE_SIZE) == 0 ) return true;

  if ( !is_blank(page, BOARD_PAGE_SIZE) && !flash_erase_page(page) ) return false;

  FLASH->SR = FLASH_ERROR_MASK;

  for ( uint32_t i = 0; i < BOARD_PAGE_SIZE / sizeof(uint32_t); i += BOARD_HALF_PAGE_SIZE / sizeof(uint32_t) )
  {
    uint32_t const status = flash_program_half_page(page + i * sizeof(uint32_t), buf + i);

    if ( status & FLASH_ERROR_MASK )
    {
      FLASH->SR = FLASH_ERROR_MASK;
      return false;
    }
  }

  return memcmp(buf, (void const*) page, BOARD_PAGE_SIZE) == 0;
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
  // writes are committed synchronously, nothing is cached
}

bool board_flash_write(uint32_t addr, void const* data, uint32_t len)
{
  // never let an image overwrite the bootloader or run off the end of flash
  if ( addr < BOARD_FLASH_APP_START || addr + len > FLASH_APP_END ) return false;
  if ( (addr | len) & 0x03 ) return false;

  if ( !flash_unlock() ) return false;

  bool ok = true;
  uint8_t const* src = data;

  while ( len )
  {
    uint32_t const page = addr & ~((uint32_t) BOARD_PAGE_SIZE - 1);
    uint32_t const offset = addr - page;
    uint32_t const count = (len < BOARD_PAGE_SIZE - offset) ? len : (BOARD_PAGE_SIZE - offset);

    if ( !flash_write_page(page, offset, src, count) )
    {
      TUF2_LOG1("Failed to write\r\n");
      ok = false;
      break;
    }

    addr += count;
    src += count;
    len -= count;
  }

  flash_lock();

  return ok;
}

void board_flash_erase_app(void)
{
  if ( !flash_unlock() ) return;

  for ( uint32_t addr = BOARD_FLASH_APP_START; addr < FLASH_APP_END; addr += BOARD_PAGE_SIZE )
  {
    if ( !is_blank(addr, BOARD_PAGE_SIZE) && !flash_erase_page(addr) ) break;
  }

  flash_lock();
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
