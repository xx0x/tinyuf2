CFLAGS += \
  -DSTM32L052xx \
  -DMSI_VALUE=2097000U

SRC_S += \
  $(ST_CMSIS)/Source/Templates/gcc/startup_stm32l052xx.s

SRC_C += \
  $(BOARD_DIR)/board.c

# For flash-jlink target
JLINK_DEVICE = stm32l052k8

flash: flash-dfu-util
erase: erase-jlink
