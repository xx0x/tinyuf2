CFLAGS += \
  -DSTM32L073xx

SRC_S += \
  $(ST_CMSIS)/Source/Templates/gcc/startup_stm32l073xx.s

SRC_C += \
  $(BOARD_DIR)/board.c

# For flash-jlink target
JLINK_DEVICE = stm32l073rb

flash: flash-dfu-util
erase: erase-jlink
