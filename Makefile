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
          -DSTM32F10X_HD \
          -DUSE_STDPERIPH_DRIVER \
          -Iinc \
          -Ilib/STM32F10x_StdPeriph_Lib_V3.6.0/Libraries/CMSIS/CM3/CoreSupport \
          -Ilib/STM32F10x_StdPeriph_Lib_V3.6.0/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x \
          -Ilib/STM32F10x_StdPeriph_Lib_V3.6.0/Libraries/STM32F10x_StdPeriph_Driver/inc

ASFLAGS := $(CPUFLAGS) -g3

LDFLAGS := $(CPUFLAGS) \
           -Tlinker.ld \
           -nostartfiles \
           -Wl,--gc-sections \
           -Wl,-Map=$(BUILD)/$(TARGET).map

SRC := \
    src/main.c \
    src/board.c \
    src/audio_clock.c \
    src/audio_dma_buffer.c \
    src/flash_authorization.c \
    src/uart.c \
    src/i2c.c \
    src/codec.c \
    src/cli.c \
    src/diagnostics.c \
    src/i2s_rx.c \
    src/i2s_meter.c \
    src/audio_stream.c \
    src/syscalls.c \
    src/system_stm32f10x.c \
    lib/STM32F10x_StdPeriph_Lib_V3.6.0/Libraries/STM32F10x_StdPeriph_Driver/src/misc.c \
    lib/STM32F10x_StdPeriph_Lib_V3.6.0/Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_gpio.c \
    lib/STM32F10x_StdPeriph_Lib_V3.6.0/Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_rcc.c \
    lib/STM32F10x_StdPeriph_Lib_V3.6.0/Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_spi.c \
    lib/STM32F10x_StdPeriph_Lib_V3.6.0/Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_dma.c \
    lib/STM32F10x_StdPeriph_Lib_V3.6.0/Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_usart.c \
    lib/STM32F10x_StdPeriph_Lib_V3.6.0/Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_i2c.c

OBJ := $(patsubst %.c,$(BUILD)/%.o,$(SRC))
OBJ += $(BUILD)/startup/startup_stm32f10x_hd.o

.PHONY: all clean size manifest manifest-test ota-test test

all: $(BUILD)/$(TARGET).bin $(BUILD)/$(TARGET).hex

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: %.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/startup/startup_stm32f10x_hd.o: startup/startup_stm32f10x_hd.s | $(BUILD)
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

manifest: $(BUILD)/$(TARGET).bin
	python3 tools/stm32_image_manifest.py $< -o $(BUILD)/$(TARGET).manifest.json

manifest-test:
	python3 -m unittest -v tests/test_stm32_image_manifest.py

test: ota-test

ota-test:
	@mkdir -p $(BUILD)/tests
	gcc -Wall -Wextra -Werror -Iinc src/flash_authorization.c tests/test_flash_authorization.c -o $(BUILD)/tests/flash_authorization
	$(BUILD)/tests/flash_authorization
	gcc -Wall -Wextra -Werror -Itests/board_stubs -Iinc src/board.c tests/test_board_lease.c -o $(BUILD)/tests/board_lease
	$(BUILD)/tests/board_lease
	gcc -Wall -Wextra -Werror -Itests/clock_stubs -Iinc src/audio_clock.c tests/test_audio_clock.c -o $(BUILD)/tests/audio_clock
	$(BUILD)/tests/audio_clock
	gcc -Wall -Wextra -Werror -Itests/clock_stubs -Iinc src/codec.c tests/test_codec_clock.c -o $(BUILD)/tests/codec_clock
	$(BUILD)/tests/codec_clock
	gcc -Wall -Wextra -Werror -Iinc src/audio_dma_buffer.c tests/test_audio_dma_buffer.c -o $(BUILD)/tests/audio_dma_buffer
	$(BUILD)/tests/audio_dma_buffer
	g++ -std=c++11 -Wall -Wextra -Werror -Itests/esp_stubs tests/test_update_transport.cpp -o $(BUILD)/tests/update_transport
	$(BUILD)/tests/update_transport
	python3 -m unittest discover -s tests -p 'test_*.py' -v

clean:
	rm -rf $(BUILD)

-include $(OBJ:.o=.d)
