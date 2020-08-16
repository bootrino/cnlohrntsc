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

#define NOOP asm volatile("nop" ::)

static void setup_clock( void )
{
	/*Examine Page 33*/

	CLKPR = 0x80;	/*Setup CLKPCE to be receptive*/
	CLKPR = 0x00;	/*No scalar*/
}

volatile unsigned char LineInfo[3*10];

void __attribute__((noinline)) DoLine() 
{
	asm volatile(
/*Step 1: Load next segment */
"L_d11%=:"		"\n\t" /* RSTART = L_d11%= */
"ld r26, %a0+"		"\n\t" /*2/0*/
"ld r25, %a0+"		"\n\t" /*2/2*/
"out %[chro], r25"	"\n\t" /*1/4*/ 
"ld r24, %a0+"		"\n\t" /*2/5*/
"out %[port], r24"	"\n\t" /*1/7*/
"tst r26"		"\n\t" /*1/0 */
"breq L_d13%="		"\n\t" /*1/1 (or two)*/
"L_d12%=:"		"\n\t" /* DOTCLOCK = L_d12%= */
"nop"			"\n\t" /*1/2 */
"nop"			"\n\t" /*1/3 */
"out %[chro], r25"	"\n\t" /*1/4 */
"dec r26"		"\n\t" /*1/5 */
/*Step 2: Loop this segment until complete, then move to next segment*/
"breq L_d11%="		"\n\t" /*1/6 */
"nop"			"\n\t" /*1/7 */
"rjmp L_d12%="		"\n\t" /*2/0 */
"L_d13%=:"		"\n\t" /* END = L_d13%= */
: 
: "e" (LineInfo), [port] "I" (_SFR_IO_ADDR(PORTA)), [chro] "I" (_SFR_IO_ADDR(USIDR)), [usisr] "I" (_SFR_IO_ADDR(USISR))
: "r0","r24", "r25", "r26"
);
}

register uint8_t castate asm("r3"); //MSB means drop 
unsigned char mydata[] PROGMEM = {
	0x00, 0x00,   56, 0x82, 0x02, 0xE5, 6, //[82] for 0x02E5, [00] for 56 - do 6 times.
	0x00, 0x02, 0xE5, 0x82, 0x00,   56, 6, //[82] for 0x02E5, [00] for 56 - do 6 times.
	0x00, 0x00,   56, 0x82, 0x02, 0xE5, 6, //[82] for 0x02E5, [00] for 56 - do 6 times.
	0x00, 0x00, 0x8C, 0x82, 0x00, 56, 12, //[82] for 0x02E5, [00] for 56 - do 6 times.
};
	

ISR(TIM1_COMPA_vect,ISR_NAKED)
{
	asm volatile(
/*Step 1: Load next segment */
"sbrs %[cas],7" "\n\t"
"reti"	"\n\t"
"push r30" "\n\t"
"push r31" "\n\t"
//"ldi r30, lo8(L_r11%=)" "\n\t"
//"ldi r31, hi8(L_r11%=)" "\n\t"
//"sts next, r30" "\n\t"
//"sts (next)+1, r31" "\n\t"
//"lds r30, %[nextx]" "\n\t"
//"lds r31, %[nextx]+1" "\n\t"
"mov r30, r4" "\n\t"
"mov r31, r5" "\n\t"
"ijmp" "\n\t"  //uncomment here to keep working
/*
"ldi r30, lo8(L_r11%=)" "\n\t"
"ldi r31, hi8(L_r11%=)" "\n\t"
"sts next, r30" "\n\t"
"sts (next)+1, r31" "\n\t"*/
//"mov r30, L_r11%=" "\n\t"
//"mov r31, (L_r11%=)+1" "\n\t"
"pop r31" "\n\t"
"pop r30" "\n\t"
"reti" "\n\t"
"L_r11%=:" "\n\t"
"nop" "\n\t"
"pop r31" "\n\t"
"pop r30" "\n\t"
"reti" "\n\t"
:
: [cas] "r" (castate), [port] "I" (_SFR_IO_ADDR(PORTA)), [chro] "I" (_SFR_IO_ADDR(USIDR)), [usisr] "I" (_SFR_IO_ADDR(USISR))
);
}

//Simply used for exiting sleep
//EMPTY_INTERRUPT(TIM1_COMPA_vect);
//EMPTY_INTERRUPT(TIM0_COMPA_vect);


const char ChromaKey[] = { 0xF0, 0xE1, 0xC3, 0x87, 0x0F, 0x1E, 0x3C, 0x78 };

int main( void )
{
	cli();

	PORTA = 0x00;
	PORTB = 0x00;
	DDRA = 0xFF;
	DDRB = 0xFF;

	setup_clock();

	//Setup SPI for high speed output.
	USICR = _BV(USIWM0) | _BV(USICS0);
	//PWM0 @ Clock rate.
	TCCR0A = _BV(WGM01);
	TCCR0B = _BV(CS00);
	OCR0A = 0;

	sei();

	//VBlank at 15.73426
	//Clock at 28363.6
	//1802.66 cycles per line
	//225.333 pixels per.
	//Let's say 225.
	unsigned char line;
	unsigned char frame=0;



	//1.1us * 3.579545 = 3.9375 (round down to 3,, 	.871 in debt)
	LineInfo[0] = 3-1; //pre-colorburst
	LineInfo[1] = 0;
	LineInfo[2] = 0x84;

	//2.5us * 3.579545 = 7.875 (round up to 10, 1.004 left over)
	LineInfo[3] = 9-1;//Colorburst
	LineInfo[4] = 0x08; 
	LineInfo[5] = 0x80;

	//1.1us * 3.579545 = 3.9375 (round up to 4,, .942 left over)
	LineInfo[6] = 3-1; //Post-colorburst
	LineInfo[7] = 0;
	LineInfo[8] = 0x84;

	//52.6 = 188.284067 (round up to 189)
	LineInfo[9] = 95-1;//(Dark Blue Bar)
	LineInfo[10] = 0x0F;
	LineInfo[11] = 0x01;

	LineInfo[12] = 94-1;//(Light Blue Bar)
	LineInfo[13] = 0xF0;
	LineInfo[14] = 0x0F;

	//1.5us * 3.579545 = 5.369 (round down to 5, .369 left over)
	LineInfo[15] = 5-1; //Front Porch
	LineInfo[16] = 0x00; //USIDR
	LineInfo[17] = 0x84; //PORTA  

	//4.7us * 3.579545 = 16.8238 (round up to 17, .192 left over)
//	LineInfo[18] = 17-1; //Sync Tip
//	LineInfo[19] = 0;
//	LineInfo[20] = 0x04;

	LineInfo[21] = 0;
	LineInfo[22] = 0x00;
	LineInfo[23] = 0x08; //End on a sync tip.


	TCCR1A = 0; //normal operation
	OCR1A = 0;
	TCCR1B = _BV(CS10);//|_BV(WGM12);
	TIMSK1 = 0;

	set_sleep_mode( SLEEP_MODE_IDLE );
	sleep_enable();

	while(1)
	{

		frame++;
		DDRA = 0xAF;

		asm volatile("ldi r16,0x80\n\tmov r3,r16": : : "r16"); // castate = 0x80 (c doesn't get it)
		for( line = 0; line < 243; line++ )	
		{
			OCR1A = 105;
			TCNT1 = 0;
			TIMSK1 = _BV(OCIE0A);

			//You have about 90 clock cycles to do things...

			LineInfo[10] = ChromaKey[(((line)>>4)&0x07)];

			sleep_cpu();
			TIMSK1 = 0;
			DoLine();

		}

		//End of frames, start of line.
		PORTA = 0;
		OCR1A = 28; //each us ~= 28.5 clocks
		TCNT1 = 0;
		TIMSK1 = _BV(OCIE0A);
	/*	asm volatile(
			"ldi r16, GoHighA\n\t"
			"mov r4, r16\n\t"
			"ldi r16, GoHighA+1\n\t"
			"mov r5, r16\n\t" : : : "r16" );
	*/
		asm volatile("ldi r16,0x00\n\tmov r3,r16": : : "r16"); //castate = 0xFF (for some reason C doesn't get it)
//		sleep_cpu();
//		TIMSK1 = 0;
//		castate = 0xab;
//		asm volatile("ldi r16, 0xff\n\tmov r3,r16\n\t": : : "r16", "memory");

		for( line = 0; line < 6; line++ )
		{
			PORTA = 0;
			_delay_us( 2 );
			PORTA = 0x82;
			_delay_us( 26 );
		}
		for( line = 0; line < 6; line++ )
		{
			PORTA = 0;
			_delay_us( 26 );
			PORTA = 0x82;
			_delay_us( 2 );
		}
		for( line = 0; line < 6; line++ )
		{
			PORTA = 0;
			_delay_us( 2 );
			PORTA = 0x82;
			_delay_us( 26 );
		}
		for( line = 0; line < 12; line++ )
		{
			PORTA = 0;
			_delay_us( 5 );
			PORTA = 0x82;
			_delay_us( 37 );
			NOOP;NOOP;NOOP;NOOP;NOOP;NOOP;NOOP;NOOP;NOOP;
		}
		_delay_us(3);
		NOOP;NOOP;NOOP;
//		while( castate != 5 ) sleep_cpu();

	}

/*
	unsigned char pixel = 0;
	while(1)
	{
		//Turns into ldi (1), out(1)
		//Must occour every eighth instruction.
		USIDR = 0x0F; 
		NOOP; //1
		NOOP; //1
		NOOP; //1
		NOOP; //1
		NOOP; //1

		//rjmp(2)
//		PORTA = 0xff;
//		PORTB = 0xfF;
//		delay_ms(200);
//		PORTA = 0x00;
//		PORTB = 0x00;
//		delay_ms(200);
	}*/

	return 0;
} 
