TARGET = directional_mic

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
ASFLAGS = \
-mcpu=cortex-m3 \
-mthumb
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(ASFLAGS) -c $< -o $@
CFLAGS = \
-mcpu=cortex-m3 \
-mthumb \
-Os \
-g3 \
-Wall \
-DSTM32F10X_XL \
-DUSE_STDPERIPH_DRIVER \
-Iinc \
-ILibraries/CMSIS/CM3/CoreSupport \
-ILibraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x \
-ILibraries/STM32F10x_StdPeriph_Driver/inc


LDFLAGS = \
-Tlinker.ld \
-nostartfiles \
-Wl,--gc-sections


SRC = \
src/main.c \
src/uart.c \
src/i2c.c \
src/cli.c \
Libraries/STM32F10x_StdPeriph_Driver/src/misc.c \
Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_gpio.c \
Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_rcc.c \
Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_spi.c \
Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_dma.c \
Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_usart.c \
Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_i2c.c 

OBJ = \
src/main.o \
src/uart.o \
src/i2c.o \
src/cli.o \
Libraries/STM32F10x_StdPeriph_Driver/src/misc.o \
Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_gpio.o \
Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_rcc.o \
Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_spi.o \
Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_dma.o \
Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_i2c.o \
Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/system_stm32f10x.o \
Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_usart.o \
startup/startup_stm32f10x_xl.o

all: $(TARGET).bin


$(TARGET).elf: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@


clean:
	rm -f src/*.o
	rm -f startup/*.o
	rm -f Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/*.o
	rm -f Libraries/STM32F10x_StdPeriph_Driver/src/*.o
	rm -f *.elf *.bin
