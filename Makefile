CC=avr-gcc
AS=$(CC)
LD=$(CC)

CPU=atmega8
AVRDUDE=avrdude -p m8 -P usb -c avrispmkII
CFLAGS=-Wall -mmcu=$(CPU) -Os -DF_CPU=8000000L
LDFLAGS=-mmcu=$(CPU) -Wl,-Map=mapfile.map

OBJS=snes2ps.o
PROG=snes2ps

# RSTDISBL  WDTON  SPIEN  CKOPT  EESAVE  BOOTSZ1  BOOTSZ0  BOOTRST
#    1        1      0      1      1        0        0        1
HFUSE=0xd9

# BODLEVEL  BODEN  SUT1  SUT0  CKSEL3  CKSEL2  CKSEL1  CKSEL0
#    1        1      1    0      0       1       0       0
LFUSE=0xE4

all: $(PROG).hex

clean:
	rm -f $(PROG).elf $(PROG).hex $(PROG).map $(OBJS)

$(PROG).elf: $(OBJS)
	$(LD) $(OBJS) $(LDFLAGS) -o $(PROG).elf

$(PROG).hex: $(PROG).elf
	avr-objcopy -j .data -j .text -O ihex $(PROG).elf $(PROG).hex
	avr-size $(PROG).elf

flash: $(PROG).hex
	$(AVRDUDE) -Uflash:w:$< -B 1.0 -e

fuse:
	$(AVRDUDE) -e -Uhfuse:w:$(HFUSE):m -Ulfuse:w:$(LFUSE):m -B 20.0 -F

erase:
	$(AVRDUDE) -B 10.0 -e

reset:
	$(AVRDUDE) -B 10.0 

%.o: %.S
	$(CC) $(CFLAGS) -c $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<
