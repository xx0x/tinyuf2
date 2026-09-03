UF2_FAMILY_ID = 0x202e3a91
CROSS_COMPILE = arm-none-eabi-

ST_HAL_DRIVER = lib/mcu/st/stm32l0xx_hal_driver
ST_CMSIS = lib/mcu/st/cmsis_device_l0
CMSIS_5 = lib/CMSIS_5

# Port Compiler Flags
CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m0plus \
  -mfloat-abi=soft \
  -nostdlib -nostartfiles \
  -DCFG_TUSB_MCU=OPT_MCU_STM32L0

# suppress warning caused by vendor mcu driver
CFLAGS += -Wno-error=cast-align -Wno-error=unused-parameter

# gcc >= 15 flags the fixed width FAT name fields in src/ghostfat.c
CFLAGS += -Wno-error=unterminated-string-initialization

# The whole bootloader has to fit in 16KB, which a debug build never does.
# Force logging off so that DEBUG=1 still produces a usable image.
ifeq ($(DEBUG), 1)
  $(warning STM32L0 port: DEBUG=1 forced to disabled due to 16KB flash size limit)
  override LOG := 0
  CFLAGS += -Os -DTUF2_LOG=0 -DCFG_TUSB_DEBUG=0
endif

# default linker file
LD_FILES ?= $(PORT_DIR)/linker/stm32l0.ld

# Port source
# Note: the flash driver is written against the FLASH/PECR registers directly
# instead of stm32l0xx_hal_flash*.c, see board_flash.c for why.
SRC_C += \
	ports/stm32l0/boards.c \
	ports/stm32l0/board_flash.c \
	$(ST_CMSIS)/Source/Templates/system_stm32l0xx.c \
	$(ST_HAL_DRIVER)/Src/stm32l0xx_hal.c \
	$(ST_HAL_DRIVER)/Src/stm32l0xx_hal_cortex.c \
	$(ST_HAL_DRIVER)/Src/stm32l0xx_hal_gpio.c \
	$(ST_HAL_DRIVER)/Src/stm32l0xx_hal_rcc.c \
	$(ST_HAL_DRIVER)/Src/stm32l0xx_hal_rcc_ex.c

ifndef BUILD_NO_TINYUSB
SRC_C += lib/tinyusb/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
endif

# Port include
INC += \
	$(TOP)/$(CMSIS_5)/CMSIS/Core/Include \
	$(TOP)/$(ST_CMSIS)/Include \
	$(TOP)/$(ST_HAL_DRIVER)/Inc
