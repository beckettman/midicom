###############################################################################
# Makefile for the project midicom (based on the V-USB-MIDI Makefile)
###############################################################################

## General Flags
PROJECT = midicom
MCU = atmega168
TARGET = $(PROJECT).elf
#DEBUG =  -DDEBUG_LEVEL=2
CC = avr-gcc
AVRDUDE = avrdude -c usbasp -p$(MCU)

LFUSE = 0xff
HFUSE = 0xdd
EFUSE = 0x01 # (default)
# Fuse low byte:
# 0xff = 1 1 1 1   1 1 1 1
#        ^ ^ \ /   \--+--/
#        | |  |       +------- CKSEL 3..0 (external >8M crystal)
#        | |  +--------------- SUT 1..0 (16K CK/14K CK + 65 ms)
#        | +------------------ CKOUT
#        +-------------------- CKDIV8
# Fuse high byte:
# 0xdd = 1 1 0 1   1 1 0 1
#        ^ ^ ^ ^   ^ \-+-/
#        | | | |   |   +------ BODLEVEL 2..0 (2.7 V)
#        | | | |   + --------- EESAVE (don't preserve EEPROM over chip erase)
#        | | | +-------------- WDTON (WDT not always on)
#        | | +---------------- SPIEN (allow serial programming)
#        | +------------------ DWEN (debugWIRE disabled)
#        +-------------------- RSTDISBL (reset pin is enabled)
# Fuse extended byte:
# 0x01 = 0 0 0 0   0 0 0 1 <-- BOOTRST (boot reset vector at 0x3800)
#                    \ /  
#                     +------- BOOTSZ 1..0 (size=1024 words)

## Options common to compile, link and assembly rules
COMMON = -g -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -Wall -DF_CPU=12000000UL -Os -fsigned-char $(DEBUG)

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += -x assembler-with-cpp -Wa,

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS +=  -Wl,-Map=$(PROJECT).map


## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom


## Include Directories
INCLUDES = -I. -Iusbdrv

## Objects that must be built in order to link
OBJECTS = usbdrv.o usbdrvasm.o oddebug.o main.o

## Objects explicitly added by the user
LINKONLYOBJECTS = 

## Build
all: $(TARGET) $(PROJECT).hex $(PROJECT).lss size

$(OBJECTS): usbconfig.h Makefile

## Compile
usbdrv.o: usbdrv/usbdrv.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

usbdrvasm.o: usbdrv/usbdrvasm.S
	$(CC) $(INCLUDES) $(ASMFLAGS) -c  $<

oddebug.o: usbdrv/oddebug.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

main.o: main.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

##Link
$(TARGET): $(OBJECTS)
	 $(CC) $(LDFLAGS) $(OBJECTS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)

%.hex: $(TARGET)
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $< $@

%.lss: $(TARGET)
	avr-objdump -h -S $< > $@

size: ${TARGET}
	@echo
	@./checksize ${TARGET}

## Clean target
.PHONY: clean
clean:
	-rm -rf $(OBJECTS) $(PROJECT).* *~


.PHONY: flash
flash:	all
	$(AVRDUDE) -U flash:w:$(PROJECT).hex


.PHONY: fuse
fuse:
	$(AVRDUDE) -U hfuse:w:$(FUSEH):m -U lfuse:w:$(FUSEL):m

