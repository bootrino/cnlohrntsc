//Copyright 2011 <>< Charles Lohr, under the MIT/x11 License.

//http://www.kolumbus.fi/pami1/video/pal_ntsc.html

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>

#define BLACK_LEVEL 0x05
#define BLANK_LEVEL 0x03
#define SYNC_LEVEL  0x00
#define WHITE_LEVEL 0x0F
#define GREY_LEVEL  0x08

#define NTSC_PORT   PORTC
#define NTSC_DDR    DDRC
#define SPI_DDR     DDRB
#define SPI_PORT    PORTB
#define SPI_MISO    4
#define SPI_MOSI    3
#define SPI_SCK     5

#define NT_NOOP asm volatile("nop" ::)

/*

//NOTE: This system is for FIRST FIELD ONLY! (i.e. 240p)

//Fs = 3.579545375 MHz
//h =                  63.55555 us (officially)
//h = 229/3.579545375  63.97461 us (us)

//Control frames  (sline = 0, cline = 1...etc)

//1-6 (DOUBLE TIME) (4.7us / 59.3us) OR (2.3us / 29.7us in reality)
//7-12 backwards
//13-18 forwards
//then, move on!

//1, 2, 3 = (low for  2.3us, high for 29.5us) (TWICE per line)
//4, 5, 6 = (low for 27.1us, high for  4.7us) (TWICE per line)
//7, 8, 9 = (low for  2.3us, high for 29.5us) (TWICE per line)

//Frames 10..19 are freebies.

//20...262 (1...243) are REAL frames (sline)   (cline = 0)
*/


volatile unsigned char cline;
volatile unsigned char sline;


//Triggered on new line
ISR(TIMER1_COMPA_vect )
{
	NTSC_PORT = SYNC_LEVEL;

	if( cline )
	{
		cline++;

		if( cline == 7 )
		{
			//Go high 27.1us later.
			OCR1B = 774;  //was 776
		}
		else if( cline == 13 )
		{
			//Go high 2.3us later
			OCR1B = 64; //was 66
		}
		else if( cline == 19 )
		{
			//Slow the clock back down.
			OCR1A = 1832;
			OCR1B = 128; //Actually /should/ be 134 but we need to preempt it.
		}
		else if( cline == 29 )
		{
			cline = 0;
			sline = 1;
		}
	}
	else
	{
		//Handle rendering of line.

		//Is this right?
		if( sline == 243 )
		{
			//Update CLINE and move timer into double-time.
			cline = 1;
			sline = 0;
			OCR1A = 1832/2;  //First bunch of lines are super quick on sync.
			OCR1B = 64;	 //With upshot quickly afterward. (2.3us) (was 66)
		}
		else
		{
			sline++;
		}
	}
}

//Partway through line  (note: only called in Vblank)
ISR(TIMER1_COMPB_vect )
{
	unsigned char s;
	unsigned char p;
	if( cline )
	{
		NTSC_PORT = BLANK_LEVEL;
	}
	else
	{
		NTSC_PORT = BLANK_LEVEL;
		_delay_us(1.0);

#define NT_NOPS		NT_NOOP; NT_NOOP;NT_NOOP;NT_NOOP; 
#define NT_NOPS2	
#define NT_NOPSB	
//NT_NOOP; NT_NOOP;NT_NOOP;
#define NT_NOPS2B	NT_NOOP; 


//#define CHROMA_LOOP SPI_PORT|=_BV(SPI_MOSI);  NT_NOPS2; SPI_PORT&=~_BV(SPI_MOSI); NT_NOPS;	
//#define CHROMA_LOOPB   NTSC_PORT&=~_BV(2); NT_NOPSB;	NTSC_PORT|=_BV(2); NT_NOPS2B;
//#define CHROMA_LOOP   NTSC_PORT|=_BV(2); NT_NOPS2;	NTSC_PORT&=~_BV(2); NT_NOPS;

//#define CHROMA_LOOP CHROMA_LOOPB

		for( s = 10; s; --s );
		{
			CHROMA_LOOP;

		//	NTSC_PORT |= _BV(2);
		//	NTSC_PORT &= ~_BV(2); NT_NOOP;
		}
		NTSC_PORT = BLANK_LEVEL;

//		CHROMA_LOOPB; CHROMA_LOOPB; CHROMA_LOOPB; CHROMA_LOOPB; CHROMA_LOOPB;
//		CHROMA_LOOPB; CHROMA_LOOPB; CHROMA_LOOPB; CHROMA_LOOPB; 

		_delay_us(1.1);

		//Below area must be <~41us

		if( sline < 100 )
		{
			NTSC_PORT = WHITE_LEVEL;
			_delay_us(10);
			NTSC_PORT = GREY_LEVEL;

/*			for( p = 0; p < 8; p++ )
			{
				SPI_DDR|=_BV(SPI_MOSI);
				CHROMA_LOOP; CHROMA_LOOP; CHROMA_LOOP; CHROMA_LOOP; CHROMA_LOOP;
				CHROMA_LOOP; CHROMA_LOOP; CHROMA_LOOP; CHROMA_LOOP; CHROMA_LOOP;
				SPI_DDR&=~_BV(SPI_MOSI);
			}*/
		for( s = 80; s; --s )
		{
		//	NTSC_PORT |= _BV(2);
		//	NTSC_PORT &= ~_BV(2); NT_NOOP;
			CHROMA_LOOP;
		}


			SPI_PORT&=~_BV(SPI_MOSI); NT_NOPS
			NTSC_PORT = BLACK_LEVEL;
		}
		else
		{
			NTSC_PORT = WHITE_LEVEL;
			_delay_us(10);
			for( s = 0; s < 9; s++ )
			{
				NTSC_PORT = GREY_LEVEL;
				_delay_us(2);
				NTSC_PORT = BLACK_LEVEL;
				_delay_us(2);
			}
			
		}
	}
}




void SetupNTSC()
{
	NTSC_DDR = 0x1F;

	//handle SPI master, etc.
//	SPI_DDR &= ~(_BV(SPI_SCK)|_BV(SPI_MOSI));
//	SPI_DDR |= _BV(SPI_MISO);
//	SPCR = _BV(SPE) ;//| _BV(MSTR) | _BV(CPHA);
//	SPSR = _BV(SPI2X);
	
	//Write to SPDR

	//Timers
	TCNT1 = 0;
	OCR1A = 1832;	//Setup line timer 1 (note this is for each line)
	OCR1B = 500;
	TCCR1A = 0;	//No PWMs.
	TCCR1B = _BV(CS10) | _BV(WGM12);  // no prescaling
	TIMSK1 = _BV(OCIE1A) | _BV(OCIE1B); //Enable overflow mark

	//Debug value
	PORTC = 0x0F;
}


