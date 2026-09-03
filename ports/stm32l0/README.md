# TinyUF2 for STM32L0

TinyUF2 reserves 16KB, therefore the application should start at `0x08004000`.

To create a UF2 image from a .bin file, either use family option `STM32L0` or its magic number as follows:

From hex

```
uf2conv.py -c -f 0x202e3a91 firmware.hex
uf2conv.py -c -f STM32L0 firmware.hex
```

From bin

```
uf2conv.py -c -b 0x08004000 -f STM32L0 firmware.bin
uf2conv.py -c -b 0x08004000 -f 0x202e3a91 firmware.bin
```

## Notes on this port

**Flash programming.** The STM32L0 NVM spends one ~3.9 ms high voltage cycle per
operation regardless of how much data that operation carries, so the driver
programs a full 64-byte half page at a time instead of a word at a time. That is
a 16x difference in throughput and it keeps individual USB MSC write commands
comfortably below the host's timeout. Half page programming is the one NVM
operation that cannot execute from flash, so `flash_program_half_page()` lives in
the `.RamFunc` section and is copied to RAM by the startup code; the linker
script folds `.RamFunc` into `.data` for that reason.

Pages whose content already matches the incoming data are skipped entirely, which
makes re-flashing an unchanged image nearly instant and makes repeated writes of
the same block safe without tracking erase state.

**End of transfer.** `board_dfu_complete()` resets immediately, as on the other
ports. It is called from inside `tud_task()` before the MSC driver re-arms the
bulk OUT endpoint, so there is no useful way for a port to hold the bus open
after the final block - waiting there only NAKs whatever the host sends next.
An OS that still had filesystem metadata queued when the last UF2 block landed
may therefore report a write error; the image is fine. Programming speed is what
keeps this rare: a device that spends a second inside a single WRITE10 command
is the one that pushes hosts into timeouts.

**Bootloader entry.** Besides the double reset tap, a board that defines
`BUTTON_PORT` / `BUTTON_PIN` / `BUTTON_STATE_ACTIVE` forces DFU mode when the
button is held during reset.

**RAM size.** [linker/stm32l0.ld](linker/stm32l0.ld) is sized for the 20KB SRAM
of the STM32L07x/L08x parts. A board on a smaller device should ship its own
script and point `LD_FILES` at it from its `board.mk`.

## Supported Boards

See the board list for this family in [supported_boards.md](../../supported_boards.md#stm32l0).
