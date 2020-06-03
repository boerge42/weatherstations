
TARGET	= main
MCU		= atmega8
F_CPU	= 12000000
CC		= avr-gcc
OBJCOPY	= avr-objcopy
AVRDUDE_PROGRAMMER = usbasp
CFLAGS	=-g -mmcu=$(MCU) -Wall -Wstrict-prototypes -Os -mcall-prologues -save-temps -fno-common -std=gnu99
CFLAGS += -DF_CPU=$(F_CPU)
EXT		= c
DOXYFILE = Doxyfile

SOURCES :=$(wildcard *.$(EXT))
OBJECTS :=$(patsubst %.$(EXT),%.o,$(SOURCES))

all: $(TARGET).hex

$(TARGET).hex : $(TARGET).elf
	$(OBJCOPY) -j .data -j .text -O ihex $^ $@
	avr-size $@

$(TARGET).elf : $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ -Wl,-Map,$(TARGET).map $^

%.o: %.$(EXT)
	$(CC) $(CFLAGS) -c $< -o $@

flash:
	avrdude -V -p $(MCU) -c $(AVRDUDE_PROGRAMMER) -U flash:w:$(TARGET).hex

doc:
	doxygen $(DOXYFILE)

.PHONY: clean
clean:
	rm -f *.i *.s $(TARGET).elf $(TARGET).hex $(TARGET).out $(TARGET).s $(TARGET).i $(TARGET).dep $(TARGET).map $(OBJECTS)
	rm -rf doc
