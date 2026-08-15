CFLAGS += \
  -DPY32F071xB

SRC_S += \
  $(PY32_TEMPLATE)/EIDE/startup_py32f071xx.s

SRC_C += \
  $(BOARD_DIR)/board.c

# For flash-jlink / flash-pyocd targets
JLINK_DEVICE = PY32F071xB
PYOCD_TARGET = py32f071xb

flash: flash-dfu-util
erase: erase-jlink
