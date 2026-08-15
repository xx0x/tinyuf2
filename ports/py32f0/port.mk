UF2_FAMILY_ID = 0x7d7a66ef
CROSS_COMPILE = arm-none-eabi-

# Puya official SDK, fetched by tools/get_deps.py
PY32_SDK = lib/mcu/puya/PY32F071_Firmware
PY32_HAL_DRIVER = $(PY32_SDK)/Drivers/PY32F071_HAL_Driver
PY32_CMSIS = $(PY32_SDK)/Drivers/CMSIS
PY32_CMSIS_DEVICE = $(PY32_CMSIS)/Device/PY32F071
PY32_TEMPLATE = $(PY32_SDK)/Templates/PY32F071xx_Templates

# Port Compiler Flags
CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m0plus \
  -mfloat-abi=soft \
  -nostdlib -nostartfiles \
  -DCFG_TUSB_MCU=OPT_MCU_PY32F0 \
  -DUSE_HAL_DRIVER

# suppress warning caused by vendor mcu driver
CFLAGS += -Wno-error=cast-align -Wno-error=unused-parameter -Wno-error=undef

# PY32F0 flash size override: force disable debug features to fit in 16KB
# Override any DEBUG=1 setting because this port is too small for debug builds
ifeq ($(DEBUG), 1)
  $(warning PY32F0 port: DEBUG=1 forced to disabled due to 16KB flash size limit)
  override LOG := 0
  CFLAGS += -Os -DTUF2_LOG=0 -DCFG_TUSB_DEBUG=0
endif

# default linker file
LD_FILES ?= $(PORT_DIR)/linker/py32f071xb.ld

# Port source
SRC_C += \
	ports/py32f0/boards.c \
	ports/py32f0/board_flash.c \
	$(PY32_TEMPLATE)/Src/system_py32f071.c \
	$(PY32_HAL_DRIVER)/Src/py32f071_hal.c \
	$(PY32_HAL_DRIVER)/Src/py32f071_hal_cortex.c \
	$(PY32_HAL_DRIVER)/Src/py32f071_hal_flash.c \
	$(PY32_HAL_DRIVER)/Src/py32f071_hal_gpio.c \
	$(PY32_HAL_DRIVER)/Src/py32f071_hal_rcc.c \
	$(PY32_HAL_DRIVER)/Src/py32f071_hal_rcc_ex.c

ifndef BUILD_NO_TINYUSB
# The PY32F07x USB device controller is a MUSB variant, not the ST FSDEV IP that
# an STM32 with a similar part number would use.
SRC_C += lib/tinyusb/src/portable/mentor/musb/dcd_musb.c
endif

# Port include
INC += \
	$(TOP)/$(PY32_CMSIS)/Include \
	$(TOP)/$(PY32_CMSIS_DEVICE)/Include \
	$(TOP)/$(PY32_HAL_DRIVER)/Inc
