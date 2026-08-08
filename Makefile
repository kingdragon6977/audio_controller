TARGET := audio_controller
BUILD := build

CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE := arm-none-eabi-size

CPUFLAGS := -mcpu=cortex-m3 -mthumb

CFLAGS := $(CPUFLAGS) \
          -Os -g3 \
          -Wall -Wextra -Wshadow -Wundef \
          -ffunction-sections -fdata-sections \
          -MMD -MP \
          -DSTM32F10X_MD \
          -DUSE_STDPERIPH_DRIVER \
          -Iinc \
          -ILibraries/CMSIS/CM3/CoreSupport \
          -ILibraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x \
          -ILibraries/STM32F10x_StdPeriph_Driver/inc

ASFLAGS := $(CPUFLAGS) -g3

LDFLAGS := $(CPUFLAGS) \
           -Tlinker.ld \
           -nostartfiles \
           -Wl,--gc-sections \
           -Wl,-Map=$(BUILD)/$(TARGET).map

SRC := \
    src/main.c \
    src/board.c \
    src/uart.c \
    src/i2c.c \
    src/cli.c \
    src/system_stm32f10x.c \
    Libraries/STM32F10x_StdPeriph_Driver/src/misc.c \
    Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_gpio.c \
    Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_rcc.c \
    Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_spi.c \
    Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_dma.c \
    Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_usart.c \
    Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_i2c.c

OBJ := $(patsubst %.c,$(BUILD)/%.o,$(SRC))
OBJ += $(BUILD)/startup/startup_stm32f10x_md.o

.PHONY: all clean size

all: $(BUILD)/$(TARGET).bin $(BUILD)/$(TARGET).hex

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: %.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/startup/startup_stm32f10x_md.o: startup/startup_stm32f10x_md.s | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJ) linker.ld | $(BUILD)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

size: $(BUILD)/$(TARGET).elf
	$(SIZE) $<

clean:
	rm -rf $(BUILD)

-include $(OBJ:.o=.d)
