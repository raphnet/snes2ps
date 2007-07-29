#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define CMD_BEGIN_01		0x01
#define CMD_GET_DATA_42		0x42
#define REP_DATA_START_5A	0x5a

#define DEVICE_ID	0x41	/* digital pad */

#define ST_IDLE			0
#define ST_READY		1
#define ST_SEND_BUF0	2
#define ST_SEND_BUF1	3
#define ST_DONE			4

/******** IO port definitions **************/
#define SNES_LATCH_DDR  DDRC
#define SNES_LATCH_PORT PORTC
#define SNES_LATCH_BIT  (1<<4)

#define SNES_CLOCK_DDR  DDRC
#define SNES_CLOCK_PORT PORTC
#define SNES_CLOCK_BIT  (1<<5)

#define SNES_DATA_PORT  PORTC
#define SNES_DATA_DDR   DDRC
#define SNES_DATA_PIN   PINC
#define SNES_DATA_BIT   (1<<3)

/********* IO pins manipulation macros **********/
#define SNES_LATCH_LOW()    do { SNES_LATCH_PORT &= ~(SNES_LATCH_BIT); } while(0)
#define SNES_LATCH_HIGH()   do { SNES_LATCH_PORT |= SNES_LATCH_BIT; } while(0)
#define SNES_CLOCK_LOW()    do { SNES_CLOCK_PORT &= ~(SNES_CLOCK_BIT); } while(0)
#define SNES_CLOCK_HIGH()   do { SNES_CLOCK_PORT |= SNES_CLOCK_BIT; } while(0)

#define SNES_GET_DATA() (SNES_DATA_PIN & SNES_DATA_BIT)

/*	PSX data : (MSb first)
		Left    Down  Right  Up    Start  1   1   Select
		Square  X     O      Tri.  R1     L1  R2  L2
*/
#define PSX_LEFT		0x8000
#define PSX_DOWN		0x4000
#define PSX_RIGHT		0x2000
#define PSX_UP			0x1000
#define PSX_START		0x0800
#define PSX_SELECT		0x0100
#define PSX_SQUARE		0x0080
#define PSX_X			0x0040
#define PSX_O			0x0020
#define PSX_TRIANGLE	0x0010
#define PSX_R1			0x0008
#define PSX_L1			0x0004
#define PSX_R2			0x0002
#define PSX_L2			0x0001

/*	SNES data, in the received order.
		B Y Select Start
		Up Down Left Right
		A X L R
		1 1 1 1
 */
#define SNES_B		0x8000
#define SNES_Y		0x4000
#define SNES_SELECT	0x2000
#define SNES_START	0x1000
#define SNES_UP		0x0800
#define SNES_DOWN	0x0400
#define SNES_LEFT	0x0200
#define SNES_RIGHT	0x0100
#define SNES_A		0x0080
#define SNES_X		0x0040
#define SNES_L		0x0020
#define SNES_R		0x0010

struct map_ent {
	unsigned short s; // Snes bit
	unsigned short p; // PSX bit
};

#define ALT_MAPPING_SNES_BIT	SNES_SELECT

static struct map_ent defaultMap[] = {
		{ SNES_B, 		PSX_X },
		{ SNES_Y, 		PSX_SQUARE },
		{ SNES_SELECT,	PSX_SELECT },
		{ SNES_START,	PSX_START },
		{ SNES_UP,		PSX_UP },
		{ SNES_DOWN,	PSX_DOWN },
		{ SNES_LEFT,	PSX_LEFT },
		{ SNES_RIGHT,	PSX_RIGHT },
		{ SNES_A,		PSX_O },
		{ SNES_X,		PSX_TRIANGLE },
		{ SNES_R,		PSX_R1 },
		{ SNES_L,		PSX_L1 },
		{ 0, 0 },
};

static struct map_ent quangMap[] = {
		{ SNES_B, 		PSX_O },
		{ SNES_Y, 		PSX_X },
		{ SNES_SELECT,	PSX_SELECT },
		{ SNES_START,	PSX_START },
		{ SNES_UP,		PSX_UP },
		{ SNES_DOWN,	PSX_DOWN },
		{ SNES_LEFT,	PSX_LEFT },
		{ SNES_RIGHT,	PSX_RIGHT },
		{ SNES_A,		PSX_R2 },
		{ SNES_X,		PSX_TRIANGLE },
		{ SNES_R,		PSX_R1 },
		{ SNES_L,		PSX_SQUARE },
		{ 0, 0 },
};


static struct map_ent *g_cur_map = quangMap;
static unsigned char state = ST_IDLE;
static volatile unsigned char psxbuf[2];
static unsigned char snesbuf[2];
static volatile char kickTransfer = 0;

static void ack()
{
	// pull acknowledge
	DDRC |= 0x01;

	_delay_us(4);

	// release acknowledge
	DDRC &= ~0x01;
}

ISR(SPI_STC_vect)
{
	unsigned char cmd;
	char ok = 1;

	cmd = SPDR;

	if (state == ST_IDLE && cmd != CMD_BEGIN_01) {
		/* First byte is no 0x01? This is not a message for us (probably memory card)
		 *
		 * Ignore all other bytes until Slave Select is deasserted.
		 */
		while (0 == (PINB & (1<<2))) { 	
			// Make sure we dont pull the bus low. No need
			// to synchronize this write with each byte transferred byte...

			SPDR = 0x00; // dont pull the bus low (sends 0xff)
		}

		return;
	}

	switch (cmd)
	{
		case CMD_BEGIN_01:
			state = ST_READY;
			SPDR = 0xff ^ DEVICE_ID;
			ack();
			break;

		case CMD_GET_DATA_42:
			if (state == ST_READY)
			{
				SPDR = 0xff ^ REP_DATA_START_5A;
				state = ST_SEND_BUF0;
				ack();
			}
			break;
		
		case 0x00:
			switch (state)
			{
				case ST_SEND_BUF0:
					SPDR = 0xff ^ psxbuf[0];
					state = ST_SEND_BUF1;
					ack();
					break;
				case ST_SEND_BUF1:
					SPDR = 0xff ^ psxbuf[1];
					state = ST_DONE;
					ack();
					break;
				case ST_DONE:
					SPDR = 0x00; // dont pull the bus low (send 0xff)
					state = ST_IDLE;
					kickTransfer = 1;
					break;

				default:
					ok = 0;
			}
		
			if (ok)
				break;

			// fallthrough
		default:
			SPDR = 0x00; // dont pull the bus low (sends 0xff)
			state = ST_IDLE;
			break;
	}

}

/* update snesbuf[] */
static void snesUpdate(void)
{
	int i,j;
	unsigned char tmp=0;

	SNES_LATCH_HIGH();
	_delay_us(12);
	SNES_LATCH_LOW();

	for (j=0; j<2; j++)
	{
		for (i=0; i<8; i++)
		{
			_delay_us(6);
			SNES_CLOCK_LOW();

			tmp <<= 1;
			if (SNES_GET_DATA())
				tmp |= 1;

			_delay_us(6);

			SNES_CLOCK_HIGH();
		}
		snesbuf[j] = tmp;
	}

}


unsigned short snes2psx(unsigned short snesbits)
{
	unsigned short psxval;
	int i;
	struct map_ent *map = g_cur_map;

	/* Start with a ALL ones message and 
	 * clear the bits when needed. */
	psxval = 0xffff;
	for (i=0; map[i].s; i++) {
		if (!(snesbits & map[i].s))
			psxval &= ~(map[i].p);
	}		

	return psxval;
}



int main(void)
{
	/* Configure /SS and MOSI as inputs, MISO
	 * as output(later) */
	DDRB = 0;

/* Enable pullup on SS. */
//	PORTB = (1<<PORTB2);

	/* Enable interrupt (SPIE)
	 * Enable SPI (SPE)
	 * Use LSB first transmission (DORD)
	 * Slave mode (MSTR not set)
	 * Clock normally high (CPOL)
	 * Data setup on leading edge (falling in this case) (CPHA)
	 * */
	SPCR = (1<<SPIE) | (1<<SPE) | (1<<DORD) | (1<<CPOL) | (1<<CPHA);
	SPDR = 0xff ^ 0xff;

	// Miso output
	DDRB = (1<<PORTB4);

	/* configure acknowledge pin. Simulate an open-collector
	 * by changing it's direction. */
	PORTC &= ~0x01;	// low
	DDRC &= ~0x01;	// input

	// buttons are active low and reserved bits stay high.
	psxbuf[0] = 0xff;
	psxbuf[1] = 0xff;

	// TODO: Snes stuff
	//
	// clock and latch as output
	SNES_LATCH_DDR |= SNES_LATCH_BIT;
	SNES_CLOCK_DDR |= SNES_CLOCK_BIT;

	// data as input
	SNES_DATA_DDR &= ~(SNES_DATA_BIT);
	// enable pullup. This should prevent random toggling of pins
	// when no controller is connected.
	SNES_DATA_PORT |= SNES_DATA_BIT;

	// clock is normally high
	SNES_CLOCK_PORT |= SNES_CLOCK_BIT;

	// LATCH is Active HIGH
	SNES_LATCH_PORT &= ~(SNES_LATCH_BIT);

	snesUpdate(); // inits buffer

	if ( ((snesbuf[0]<<8 | snesbuf[1]) & 
				ALT_MAPPING_SNES_BIT) == 0) {
		g_cur_map = defaultMap;
	}
	
	sei();
	while(1)
	{
		unsigned short psxbits;

		while (!kickTransfer) 
			{ /* do nothing */	}

		kickTransfer = 0;
		snesUpdate();

		psxbits = snes2psx((snesbuf[0]<<8) | snesbuf[1]);

		psxbuf[0] = psxbits >> 8;	
		psxbuf[1] = psxbits & 0xff;
	}

}

