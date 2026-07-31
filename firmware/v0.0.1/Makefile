# Makefile for CANnula v0.0.1 (Vulnerable Version)

TARGET = cannula_v0.0.1_vulnerable
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

# Intentionally weak compiler flags
CFLAGS = -mcpu=cortex-m3 -mthumb
CFLAGS += -O0  # No optimization
CFLAGS += -fno-stack-protector  # Disable stack protection
CFLAGS += -fno-pie  # No PIE
CFLAGS += -Wno-format-security  # Ignore format string warnings
CFLAGS += -g  # Debug symbols

LDFLAGS = -mcpu=cortex-m3 -mthumb
LDFLAGS += -Tlink.ld
LDFLAGS += -nostartfiles
LDFLAGS += -Wl,--no-warn-rwx-segments  # Allow RWX segments

SRCS = vulnerable_main.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET).hex $(TARGET).bin

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^
	$(SIZE) $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o *.elf *.hex *.bin

flash: $(TARGET).hex
	"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" -c port=SWD -w $(TARGET).hex -v -rst

.PHONY: all clean flash