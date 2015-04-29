/*
 * lcd.c
 *
 *  Modified on: April 7, 2014
 *       Author: Roscoe Giles
 *
 * Example code for interfacing with an LCD panel from the MSP430
 *
 * The LCD Panel used is an AMOTEK ADM1602K (16x2, black on green, 3.3V)
 * with a Sitronix ST7066 controller. (packaged by SPARKFUN as part LCD-09053).
 *
 * The MSP430G2553 connects directly to the Panel (using 7 I/O pins!)
 * 6 of the I/O pins are the low 6 pins of the "LCDPORT" ( P2.5...P2.0)
 * the 7th pin is the "clock" signal that controls the LCD's data transfers. (P1.0)
 *
 * CONNECTIONS:
 *
 * MSP430			  			Panel
 * ------                       -----
 * LCDPORT.3...LCDPORT.0: 		DB7...DB4 (data lines)
 * LCDPORT.4				    RS (register select)
 * LCDPORT.5				    RW (read-write select)
 * CONTROLPORT.0                E (enables read/write)
 *
 * LCDPORT Bits:
 * Bit:         7   6   5   4   3   2   1   0
 * Controller:  x   x   RW  RS  D7  D6  D5  D4
 *
 * CONTROLPORT Bits:
 * Bit:         7   6   5   4   3   2   1   0
 * Controller:  x   x   x   x   x   x   x   E

 * TIMING:
 * There are a number of time delay required by the panel.
 * The code assumes an 8 MHz SMCLK which is divided down by 8 to drive the TA1 clock
 * at 1 MHz in continuous mode.  Timing of delays is done with interrupts on TA1CCR0.
 *
 * All the delay times are in microseconds.
 * Parameter ENABLE_PULSE_TICKS is the number of microseconds (minimum) that the enable
 * pulse to the LCD is held high or low.  Fewer than about 10 or 15 us is too fast for the
 * handler.
 *
 */

#include <msp430g2553.h>
#include <string.h>

// Timing parameters
#define ENABLE_PULSE_TICKS 20

// ************ Port definitions ***************
#define LCDPORT(x) P2##x
#define CONTROLPORT(x) P1##x
/* bit masks for LCDPORT */
#define DATABITS 0x0F
#define RW 0x20
#define RS 0x10
// Shifted bit positions for RS and RW flags in the encoded integer argument to LCD_put
#define RSINT (16*RS)
#define RWINT (16*RW)
/* bit mask for CONTROLPORT */
#define ENABLE_READ 0x01


void init_LCD_ports(){
	// setup ports for LCD
	LCDPORT(OUT) &= ~(DATABITS+RW+RS);
	LCDPORT(DIR)|=DATABITS+RW+RS;
	CONTROLPORT(OUT) &= ~ENABLE_READ;
	CONTROLPORT(DIR) |= ENABLE_READ;
}

/* LCD PANEL INTERACTIONS
 * LCD_put_4(data) writes 4 bits to the LCD, using the current
 *           RS and RW bits in the output registers
 * start_LCD_put_4() does the same but DOES NOT wait to return
 *
 * LCD_put(int value) expects a 9 bit value consisting of RS + 8 Data bits
 *   RS D7 D6 D5 D4 D3 D2 D1 D0
 * start_LCD_put() does the same but DOES NOT wait to return
 *           bit 8: RS (register select)
 *           bits 7...0: 8 bit data value
 *
 * LCD_SM_busy() returns non-zero if the LCD state machine (driven by TA1) is busy
 *
 * start_LCD_send_char(char c) starts sending a single character to LCD register
 * start_LCD_send_buffer(char *data, int count) sends a character array
 * start_LCD_send_string(char *s) sends a string
 *
 * LCD_send_string(char *s) puts the characters 1 by 1 to the LCD
 *           data register (RS=1) [ and waits between characters ]
 *           (this is mainly for demo purposes)
 *
 */



/*
 * TA1 CONTROLLED STATE MACHINE
 *
 * Each of the main functions that interact with LCD panel has timing constraints.
 * In order to take the timing into account efficiently, the underlying operations
 * are all invoked by the TA1CCR0 interrupt handler.
 *
 * Each function is realized by a state machine with its own transitions.
 * The state is represented by the variables
 *     current_LCD_function -- number identifying the current function
 *     LCD_step             -- number identifying the next step in performing the
 *                                    current function
 *
 * The state machines are:
 * SM_INACTIVE: indicates that no function is in progress
 * SM_PUT_4: sending a 4 bit value to the LCD (either command or data register)
 * SM_PUT: sending an 8 bit value to either the command or data register
 * SM_DELAY_HACK: allows for a time delay function delay() that is used by the
 *                current version of LCD_Init();
 * SM_SEND_BUFFER: for sending a character array
 */

#define SM_INACTIVE 0
#define SM_PUT_4    1
#define SM_PUT      2
#define SM_SEND_BUFFER 3
#define SM_DELAY_HACK 4

int current_LCD_function = SM_INACTIVE; // which of various state machines are we in?
int LCD_step; // next step for the current machine. step = 0 always means deactivate.

int LCD_SM_busy(){
	return (current_LCD_function != SM_INACTIVE);
}

void init_TA1(){ // setup TA1
	TA1CTL = TACLR;
	TA1CTL = TASSEL_2+ID_3+MC_2;
	TA1CCTL0=0; // compare mode interrupt disabled
}

void start_timer(unsigned int n){
	TA1CCR0 = TA1R+n;
	TA1CCTL0 &=~ CCIFG; // clear flag just in case
	TA1CCTL0 |= CCIE; // enable interrupt
}

inline void stop_timer(){
	TA1CCTL0=0;
}


/*
 * First the "delay hack"
 * start_delay initiates a delay which is implemented by TA1
 */

void start_delay(unsigned int n){ // this is a hack to allow busy waiting
	current_LCD_function=SM_DELAY_HACK;
	LCD_step=1;
	start_timer(n);
}

void sm_wait(){
	while (LCD_SM_busy());
}

void delay(unsigned int n){
	start_delay(n);
	sm_wait();
}

/* *************************************************
 * LCD_put_4 state machine
 */

void start_LCD_put_4(unsigned char data){  // write 4 data bits keeping current RS flag
	current_LCD_function=SM_PUT_4;
	LCD_step=1;
	LCDPORT(OUT) = (LCDPORT(OUT) &~ DATABITS)| (data & DATABITS); // replace DATA bits in output
	CONTROLPORT(OUT)|=ENABLE_READ; // latch data into controller
	start_timer(ENABLE_PULSE_TICKS);
}

int LCD_put_4_action(int s){ // update the state for LCD_put_4 operation
	int next_s;
	switch (s) {
		case 1: { // now it is time to lower the enable pulse
			CONTROLPORT(OUT)&= ~ENABLE_READ;
			next_s=2; // hold time for signal down
			TA1CCR0 += ENABLE_PULSE_TICKS;
			break;
		}
		case 2: { // hold time is over
			next_s=0; // interpreted by the handler as time to cleanup
			break;
		}
	}
	return next_s;
}

/*
 * Now a version of LCD_put_4 that looks like a function (does not return until done)
 */

void LCD_put_4(unsigned char data){
	start_LCD_put_4(data);
	sm_wait(); // wait for completion
}


/******************************************************************
 * LCD_put( R/S D7 D6 ... D1) writes to a register of of the LCD
 *          selected by the R/S bit (0 = control, 1 = data)
 */

volatile int LCD_current_put_value; // a global to share current data word

void init_put_word(unsigned int value){ // start the send of the current word (code shared by LCD_put and LCD_send_buffer
	unsigned char high;
	LCD_current_put_value=value;
	LCD_step=1;
	high = value/16;  // 0 0 0 RS DB7 DB6 DB5 DB4 (R/W bit is zero)
	LCDPORT(OUT) = ( LCDPORT(OUT) &0xC0) | (high & 0x1F) ; // send out 6 high bits to lines
	CONTROLPORT(OUT)|=ENABLE_READ; // latch data into controller
	start_timer(ENABLE_PULSE_TICKS); // start the rest of the state machine
}
void start_LCD_put(int value){
	current_LCD_function=SM_PUT;
	init_put_word(value);
}

// a simple wrapper to put a single character.
void start_LCD_write_char(char c) {
     start_LCD_put(RSINT+c);
}

int LCD_put_action(int s){
	unsigned char low;
	int next_s;
	switch(s){
		case 1: { // time to lower the enable line
			CONTROLPORT(OUT)&= ~ENABLE_READ;
			next_s=2; // hold time for signal down
			TA1CCR0 += ENABLE_PULSE_TICKS;
			break;
		}
		case 2: { // compute low 4 bits and send
			low = (LCD_current_put_value & 0x0F) ;// DB3 DB2 DB1 DB0
			LCDPORT(OUT) = (LCDPORT(OUT) & 0xF0) | low;
			CONTROLPORT(OUT)|=ENABLE_READ; // latch data into controller
			TA1CCR0 += ENABLE_PULSE_TICKS;
			next_s=3;
			break;
		}
		case 3: {// lower enable
			CONTROLPORT(OUT)&= ~ENABLE_READ;
			next_s=4; // hold time for signal down
			TA1CCR0 += ENABLE_PULSE_TICKS;
			break;
		}
		case 4: {
			next_s=0; // interpreted by the handler as time to cleanup
			break;
		}
	}
	return next_s;
}

void LCD_put(int value){ // a version that waits
	start_LCD_put(value);
	sm_wait();
}

/* ***************************************
 * NEW
 * A special machine to put an array without busy waiting
 * start_LCD_send_buffer(char *data, int count)
 *
 * Note: the continuation function for this shares code with LCD_put_action
 */

// global variables for LCD_send_buffer
char *LCD_buffer_ptr;	 // next character in the buffer
int LCD_char_count;      // remaining number of characters to send

void start_LCD_send_buffer(char *data, int count){
	LCD_buffer_ptr=data;
	LCD_char_count=count;
	if((LCD_char_count)>0){
		current_LCD_function=SM_SEND_BUFFER;
		init_put_word(RSINT+ (*LCD_buffer_ptr++));
		--LCD_char_count;
	}
}

void start_LCD_send_string(char *s){
	start_LCD_send_buffer(s, strlen(s));
}

/*
 * The TA1 handler runs all the state machines for the LCD data transfer
 */

interrupt void TA1CCR0_handler(){
	int next_s;
	switch (current_LCD_function) {
		case SM_INACTIVE: { // should not happen if things are well designed
			next_s=0;
			break;
		}
		case SM_PUT_4: {
			next_s=LCD_put_4_action(LCD_step);
			break;
		}
		case SM_PUT: {
			next_s=LCD_put_action(LCD_step);
			break;
		}
		case SM_DELAY_HACK:{
			next_s=0;
			break;
		}
		case SM_SEND_BUFFER:{
			next_s=LCD_put_action(LCD_step); // action is the same as put
		}
	}
	LCD_step=next_s;

	// cleanup for all machines
	if (next_s==0){
		if((current_LCD_function==SM_SEND_BUFFER) && (LCD_char_count>0)){
			// maybe go back for more characters in special case of sending buffer
			init_put_word(RSINT+ (*LCD_buffer_ptr++));
			--LCD_char_count;
		} else {
			current_LCD_function = SM_INACTIVE;
			stop_timer(); // turn off TA1 interrupt
		}
	}
}
ISR_VECTOR(TA1CCR0_handler, ".int13")

/*
void LCD_send_string (char *s){
	char c;
	while ((c = *s++)!=0){
		start_LCD_write_char(c);
		sm_wait();
	}
}
*/

void LCD_send_string(char *s){
	start_LCD_send_string(s);
	sm_wait();
}

void LCD_init(){
	delay(15000); // insure that 1.5 ms have elapsed
	// 3 function sets with delays
	LCD_put_4(3);
	delay(4100);
	LCD_put_4(3);
	delay(100);
	// set mode to 4 bit mode
	LCD_put_4(3);
	LCD_put_4(2);
	delay(100);
	// now we can send using the regular instructions
	LCD_put(0x28); // 2 lines, 5x8 characters
	LCD_put(0x0F); // display, cursor, blink on
	LCD_put(1);    // clear display
	delay(16000); // must wait 1.52ms after clear!
}

void LCD_setup(){
	init_TA1();
  	init_LCD_ports();
}
