# TinyUF2 for PY32F0

Support for Puya PY32F07x, currently the PY32F071xB (128KB flash, 16KB SRAM,
Cortex-M0+ at up to 72MHz).

Despite the similar part number, a PY32F071 is **not** an STM32F071. In
particular the USB device controller is a MUSB variant rather than the ST FSDEV
IP, and flash is programmed one 256-byte page at a time. This port therefore
uses Puya's own [PY32F071_Firmware](https://github.com/OpenPuya/PY32F071_Firmware)
SDK, fetched automatically by `tools/get_deps.py`.

TinyUF2 reserves 16KB, therefore the application should start at `0x08004000`.

## Clocking

The USB device is clocked from the system clock, which must be exactly 48MHz.
The boards here trim the internal HSI to 24MHz and run it through the PLL (x2),
so no external crystal is required.

## Build

```
make BOARD=badger_py32f071 get-deps
make BOARD=badger_py32f071 all
```

## Creating UF2 images

Use family option `PY32F071-UVK5-V3` or its magic number `0x7d7a66ef`. This is
the only PY32F071 family ID registered in
[microsoft/uf2](https://github.com/microsoft/uf2/blob/master/utils/uf2families.json);
the name refers to the first board to claim it, but the ID identifies the MCU.

From hex

```
uf2conv.py -c -f 0x7d7a66ef firmware.hex
```

From bin

```
uf2conv.py -c -b 0x08004000 -f 0x7d7a66ef firmware.bin
```

## Supported Boards

See the board list for this family in [supported_boards.md](../../supported_boards.md#py32f0).
