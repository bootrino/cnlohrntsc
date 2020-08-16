#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

void delay_ms(uint32_t time) {
  uint32_t i;
  for (i = 0; i < time; i++) {
    _delay_ms(1);
  }
}

void SetupNTSC();

#define NOOP asm volatile("nop" ::)

volatile unsigned char cline, sline;

static void setup_clock( void )
{
	/*Examine Page 33*/

	CLKPR = 0x80;	/*Setup CLKPCE to be receptive*/
	CLKPR = 0x00;	/*No scalar*/
}

int main( void )
{
	cli();

	PORTA = 0x00;
	PORTB = 0x00;
	DDRA = 0xAF;
	DDRB = 0xFF;

	setup_clock();


	//Setup SPI for high speed output.
	USICR = _BV(USIWM0) | _BV(USICS0);
	TCCR0A = _BV(WGM01);
	TCCR0B = _BV(CS00);
	OCR0A = 0;

	SetupNTSC();
	sei();

	while(1)
	{
			sleep_cpu();

	}

	return 0;
} 

#define BLACK_LEVEL 0x81
#define BLANK_LEVEL 0x80
#define SYNC_LEVEL  0x00
#define WHITE_LEVEL 0xFF
#define GREY_LEVEL  0xFF

#define NTSC_PORT   PORTA
#define NTSC_DDR    DDRA
//#define SPI_DDR     DDRB
//#define SPI_PORT    PORTB
#define SPI_MISO    4
#define SPI_MOSI    3
#define SPI_SCK     5

#define NT_NOOP asm volatile("nop" ::)



//Triggered on new line
ISR(TIM1_COMPA_vect)
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
ISR(TIM1_COMPB_vect )
{
	unsigned char s;
//	unsigned char p;
	if( cline )
	{
		NTSC_PORT = BLANK_LEVEL;
	}
	else
	{
		NTSC_PORT = BLANK_LEVEL;
		_delay_us(1.1);

#define NT_NOPS		
#define NT_NOPS2	NT_NOOP; 

#define CHROMA_LOOP  \
	NTSC_PORT|=_BV(5); NT_NOPS2; \
	NTSC_PORT&=~_BV(5); NT_NOPS;

//#define CHROMA_LOOPB \
//	NTSC_PORT|=_BV(5); NT_NOOP; NT_NOOP;\
//	NTSC_PORT&=~_BV(5); NT_NOPS;NT_NOOP;

#define CHROMA_LOOPB USIDR = 0x07;NT_NOOP; NT_NOOP; NT_NOOP; NT_NOOP; NT_NOOP; NT_NOOP; 
#define CHROMA_LOOPC USIDR = 0x07;NT_NOOP; NT_NOOP; NT_NOOP; NT_NOOP; NT_NOOP; NT_NOOP; 


//		NTSC_DDR|=_BV(5);
		NTSC_PORT = BLANK_LEVEL;

		CHROMA_LOOPB;CHROMA_LOOPB;CHROMA_LOOPB;CHROMA_LOOPB;CHROMA_LOOPB;
		CHROMA_LOOPB;CHROMA_LOOPB;CHROMA_LOOPB;CHROMA_LOOPB;CHROMA_LOOPB;
		USIDR = 0x00;

		NTSC_PORT = BLANK_LEVEL;

		_delay_us(1.1);
		NTSC_PORT = GREY_LEVEL;

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
		NOOP; NOOP; NOOP; NOOP; NOOP; NOOP; NOOP;
		for( s = 5; s; --s )
		{
			CHROMA_LOOPC;CHROMA_LOOPC;CHROMA_LOOPC;CHROMA_LOOPC;CHROMA_LOOPC;
			CHROMA_LOOPC;CHROMA_LOOPC;CHROMA_LOOPC;CHROMA_LOOPC;CHROMA_LOOPC;
			CHROMA_LOOPC;CHROMA_LOOPC;CHROMA_LOOPC;CHROMA_LOOPC;CHROMA_LOOPC;
		}
		USIDR = 0x00;



//			SPI_PORT&=~_BV(SPI_MOSI); NT_NOPS
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
	NTSC_DDR = 0xAF;

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
//	PORTC = 0x0F;

}


