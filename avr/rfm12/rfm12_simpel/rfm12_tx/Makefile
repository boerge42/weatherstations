# 
#
#

MCU		     = atmega8
TARGET_ARCH  = -mmcu=$(MCU)
OBJECTS      = main.o uart.o rf12.o 
AVRDUDE_PROGRAMMER = usbasp
TARGET	= main


CC      = avr-gcc
CFLAGS  = -Os -g -Wall -std=gnu99 -I. 
CFLAGS  += -fno-strict-aliasing
CFLAGS  += -finline-limit=4 -fno-if-conversion -mcall-prologues
CFLAGS  += -funsigned-char  -funsigned-bitfields -fpack-struct -fshort-enums
# -fno-tree-scev-cprop and -fno-split-wide-types since avr-gcc 4.3.0
CFLAGS  += -fno-tree-scev-cprop -fno-split-wide-types 

CFLAGS  += -DF_CPU=12000000UL
ASFLAGS = -Os -g -Wall -I.
LDFLAGS = -g
#LDFLAGS += -Wl,--relax
MODULES = $(OBJECTS)


all:	$(TARGET).hex

clean:
	rm -f *.hex *.elf  $(TARGET).elf *.o *.d 

$(TARGET).elf: $(MODULES)
	$(LINK.o) -o $@ $(MODULES)

$(TARGET).hex: $(TARGET).elf
	avr-objcopy -j .text -j .data -O ihex $(TARGET).elf $(TARGET).hex
	avr-size $(TARGET).hex

disasm: $(TARGET).elf
	avr-objdump -S $(TARGET).elf

flash:
	avrdude -p $(MCU) -c $(AVRDUDE_PROGRAMMER) -U flash:w:$(TARGET).hex	

$(TARGET).o: $(TARGET).c

.PHONY: pretty_code
pretty_code:
	rm -f *.hex *.elf  $(TARGET).elf *.o *.d 
	find */*.c -type f -print0 | xargs -0 fromdos -p
	find */*.h -type f -print0 | xargs -0 fromdos -p
	find */*.bas -type f -print0 | xargs -0 fromdos -p
	fromdos -p Makefile*
	fromdos -p *.h
	fromdos -p *.c
	find . -type f -print0 | xargs -0 chmod 644
