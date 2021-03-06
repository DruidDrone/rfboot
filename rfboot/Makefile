# Makefile for the rfboot bootloader

# program name. Should not be changed.
PROGRAM    = rfboot

include hardware_settings.mk

OBJ        = $(PROGRAM).o
OPTIMIZE   = -Os -fno-inline-small-functions -fno-split-wide-types
DEFS       = -g
LIBS       = -Icc1101 -Ixtea
CC         = avr-gcc

# Default frequency is 8Mhz, unless you set one in "complile_settings.mk"
# This should be the normal for atmega328 @ 3.3V
ifeq ($(F_CPU),)
F_CPU := 8000000L
endif

# Default programmer is usbasp
ifeq ($(PROGRAMMER),)
PROGRAMMER := usbasp
endif



# Default is no crystal
ifeq ($(CRYSTAL),1)
LFUSE := 0xFF
else
LFUSE := 0xE2
endif

COMPILE_TIME := $(shell date '+%s')

# Override is only needed by avr-lib build system.
override CFLAGS        = -Wall $(OPTIMIZE) -mmcu=$(MCU_TARGET) -DF_CPU=$(F_CPU) $(DEFS)
override LDFLAGS       = -Wl,$(LDSECTION)

OBJCOPY        = avr-objcopy
OBJDUMP        = avr-objdump

all: atmega328p

atmega328p: check
atmega328p: clean
atmega328p: MCU_TARGET = atmega328p
atmega328p: CFLAGS += -std=gnu99 -Wall -ffunction-sections -fdata-sections -fshort-enums -g -Os -w -fno-exceptions -Wl,--gc-sections -Ixtea -Icc1101
atmega328p: CFLAGS += $(OSCCAL_FLAG)
atmega328p: CFLAGS += -DCOMPILE_TIME=$(COMPILE_TIME)
atmega328p: LDSECTION  = --section-start=.text=0x7000
atmega328p: $(PROGRAM)_atmega328p.elf
atmega328p: size

isp: getosccal atmega328p
	avrdude -qq -p atmega328p -c $(PROGRAMMER) -e -U lfuse:w:$(LFUSE):m -U hfuse:w:0xD8:m -U efuse:w:0xFD:m -U flash:w:rfboot_atmega328p.elf -U lock:w:0x0C:m

%.elf:
	$(CC) $(CFLAGS) $(LDFLAGS) -c -o xtea.o xtea/xtea.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c -o cc1101.o cc1101/cc1101.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c -o spi.o cc1101/spi.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c -o rfboot.o rfboot.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ rfboot.o xtea.o spi.o cc1101.o

check:
	@test -s rfboot_settings.h || { echo "rfb_settings.h does not exist ! Exiting..."; exit 1; }

clean:
	rm -rf *.o *.elf *.lst *.map *.sym *.lss *.eep *.srec *.bin *.hex

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

size:
	avr-size --mcu=$(MCU_TARGET) -C $(PROGRAM)_$(MCU_TARGET).elf

ifdef RC_CALIBRATOR
getosccal:
	$(eval OPTIMAL_OSCCAL_VALUE := $(shell $(RC_CALIBRATOR) ) )
	$(eval OSCCAL_FLAG := -DOPTIMAL_OSCCAL_VALUE=$(OPTIMAL_OSCCAL_VALUE) )
else
getosccal:

endif
