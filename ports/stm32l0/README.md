# TinyUF2 for STM32F3

TinyUF2 reserved 16KB therefore application should start at `0x08004000`

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

## Supported Boards

See the board list for this family in [supported_boards.md](../../supported_boards.md#stm32l0).
