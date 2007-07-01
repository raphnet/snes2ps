CC=avr-gcc
AS=$(CC)
LD=$(CC)

CPU=atmega8
UISP=uisp -dprog=stk500 -dpart=atmega8 -dserial=/dev/avr
AVRDUDE=avrdude -p atmega8 -c avrisp -P /dev/avr
CFLAGS=-Wall -mmcu=$(CPU) -Os -DF_CPU=8000000UL
LDFLAGS=-mmcu=$(CPU) -Wl,-Map=mapfile.map

OBJS=snes2ps.o
PROG=snes2ps

all: $(PROG).hex

clean:
	rm -f $(PROG).elf $(PROG).hex $(PROG).map $(OBJS)

$(PROG).elf: $(OBJS)
	$(LD) $(OBJS) $(LDFLAGS) -o $(PROG).elf

$(PROG).hex: $(PROG).elf
	avr-objcopy -j .data -j .text -O ihex $(PROG).elf $(PROG).hex
	avr-size $(PROG).elf

fuse:
	$(UISP) --wr_fuse_h=0xc9 --wr_fuse_l=0xe4

flash: $(PROG).hex
	$(AVRDUDE) -U flash:w:$(PROG).hex

%.o: %.S
	$(CC) $(CFLAGS) -c $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<
