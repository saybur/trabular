##### PER PROJECT CONFIGURATION OPTIONS #####

PROGRAMMER := arduino -P /dev/ttyUSB0 -b 57600 -D
#PROGRAMMER := usbtiny

MCU := atmega328p

ADB_PORT := ADB_PORTB
ADB_DATA_PIN := 0
FEATURES := -DUSE_MOUSE -DUSE_KEYBOARD -DUSE_ARBITRARY
FEATURES += -DUSE_USART
#FEATURES += -DDEBUG_MODE

##### GENERAL CONFIGURATION OPTIONS #####

F_CPU := 16000000

WARNINGS := -Wall -Wextra -pedantic
CC := avr-gcc
CFLAGS ?= -std=c99 $(WARNINGS) -Os -mmcu=$(MCU) -DF_CPU=$(F_CPU) \
			-D$(ADB_PORT) -DADB_DATA_PIN=$(ADB_DATA_PIN) \
			$(FEATURES)
AVRDUDE_FLAGS := -p $(MCU) -c $(PROGRAMMER)

MAIN = program
SRCS = ring.c registers.c serial.c adb.c main.c
OBJS = $(SRCS:.c=.o)

.PHONY: all
all: $(MAIN).hex

.PHONY: clean
clean:
	rm -f $(MAIN).elf $(MAIN).hex $(MAIN).lst $(OBJS)

.PHONY: flash
flash: $(MAIN).hex
	avrdude $(AVRDUDE_FLAGS) -U $<

.PHONY: dump
dump: $(MAIN).elf
	avr-objdump -d -S -m avr $(MAIN).elf > $(MAIN).lst

$(MAIN).hex: $(MAIN).elf
	avr-objcopy -j .text -j .data -O ihex $< $@

$(MAIN).elf: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)
	avr-size -C --mcu=$(MCU) $(MAIN).elf
