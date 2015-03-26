asm(" .length 6000");
asm(" .width 132");
// Mar 2015
// Tone == produces a 1 Khz tone using TimerA, Channel 1
// Toggled on and off with a button
// Using the timer in continuous mode, with the handler updating the
// 'alarm' time.
// Sound is turned on and off by directly manipulating
// the TACCTL0 register.  The half period is not dynamically
// updated (though it can be changed in the debugger by
// changing the global variable half_period.
// The pushbutton is not debounced in any way!
// CPU clock --> 8Mhz

#include "msp430g2553.h"
//-----------------------
// The following definitions allow us to adjust some of the port properties
// for the example:

// define the bit mask (within P1) corresponding to output TA1
#define TA0_BIT 0x02

// define the location for the button (this is the built in button)
// specific bit for the button
#define START 0x08		//P1.3 (start and pause)
#define SPEEDUP 0x10	//P1.4
#define SLOWDOWN 0x20	//P1.5
#define REPLAY 0x80 	//P1.7
#define SELECT 0x04		//P1.2
//P1.6 doesn't work, probably because it's the green LED
//----------------------------------

// Some global variables (mainly to look at in the debugger)
volatile unsigned soundOn=0; // state of sound: 0 or OUTMOD_4 (0x0080)
volatile unsigned speedup=0;
volatile unsigned slowdown=0;
volatile unsigned replay=0;
volatile unsigned select=1;
volatile int index = 0;
volatile double rate = 1.0;
volatile int size = 57; //set both of the songs to the same size
const int song[2][57] = {
		//song[0] is Joy of the World
		{659, 698, 784, 880, 989, 1046, 1175, 1318, 880, \
		784, 784, 698, 698, 659, 659, 659, 698, 784, 880, 880, 989, 1046, 659, \
		659, 698, 784, 880, 880, 989, 1046, 1046, 1046, 1046, 1046, 989, 880, 989, 1046, 1175, 1175, 1175, 1175, 1046, 989, \
		1046, 1175, 1046, 659, 784, 880, 989, 1046, 989, 1046, 1175, 1318},
		//song[1] is my own song
		{1174, 1046, 989, 1318, 1318, 880, 989, 1046, 1174, 1174, 1174, 1174, 1174, 1046, 989, 880, 989, \
		1174, 1046, 989, 1318, 784, 880, 989, 880, 784, 784, 784, 740, 784, 880, 989, \
		659, 784, 880, 989, 989, 659, 784, 989, 989, 989, 1046, 1318, 1318, \
		1480, 1480, 1568, 1480, 1568, 1480, 1480, 1568, 1976, 0, 0, 0}
		};
const int notes[2][57] = {
		//notes[0] is the notes for Joy of the World
		{1200, 900, 300, 1800, 600, 1200, 1200, 1800, 600, \
		1800, 600, 1800, 600, 1800, 600, 600, 600, 600, 600, \
		900, 300, 600, 600, 600, 600, 600, 600, \
		900, 300, 600, 600, 600, 600, 600, 300, 300, \
		1800, 300, 300, 600, 600, 600, 300, 300, \
		1800, 300, 300, 600, 1200, 600, \
		900, 300, 600, 600, 1200, 1200, 2400},
		//notes[1] is the notes for my own song
		{300, 300, 1800, 300, 300, 1800, 300, 300, 300, 300, \
		300, 300, 300, 300, 600, 300, 1500, 300, 300, 1800, \
		300, 300, 1800, 300, 300, 300, 300, 300, 900, 300, \
		300, 1800, 900, 900, 1800, 600, 600, 900, 900, \
		1800, 600, 600, 900, 900, 1800, 600, 300, 300, 300, \
		300, 300, 300, 300, 300, 300, 300, 300}
		};
volatile int state = 0;		//the current state (which frequency it is playing)
volatile int current = 0;	//the current note


void init_timer(void); // routine to setup the timer
void init_button(void); // routine to setup the button

// ++++++++++++++++++++++++++
void main(){
	WDTCTL = WDTPW + WDTHOLD; // Stop watchdog timer
	BCSCTL1 = CALBC1_1MHZ; // 1Mhz calibration for clock
	DCOCTL = CALDCO_1MHZ;

	init_timer(); // initialize timer
	init_button(); // initialize the button
	_bis_SR_register(GIE+LPM0_bits);// enable general interrupts and power down CPU
}

// +++++++++++++++++++++++++++
// Sound Production System
void init_timer(){ // initialization and start of timer
	TA0CTL |=TACLR; // reset clock
	TA0CTL =TASSEL1+ID_0+MC_2; // clock source = SMCLK, clock divider=1, continuous mode,
	TA0CCTL0=soundOn+CCIE; // compare mode, outmod=sound, interrupt CCR1 on
	//TA0CCR0 = TAR+song[state]; // time for first alarm
	P1SEL|=TA0_BIT; // connect timer output to pin
	P1DIR|=TA0_BIT;
}

// +++++++++++++++++++++++++++
void interrupt sound_handler(){
	TA0CCR0 = TAR+song[index][state];
	if(replay){
		state = 0;
		current = 0;
		rate = 1.0;
		replay &= ~replay;		//clear replay
	}
	if(speedup){
		rate = rate / 2;
		speedup &= ~speedup;	//clear speedup
	}
	if(slowdown){
		rate = rate * 2;
		slowdown &= ~slowdown; 	//clear slowdown
	}
	if (select){
		if(index == 0){
			index = 1;
		}
		else if(index == 1){
			index = 0;
		}
		state = 0;
		current = 0;
		rate = 1.0;
		select &= ~select;		//clear select
	}
	if (soundOn){
		if(state == 0 && current == 0)
		{
			current = notes[index][state] * rate;
		}
		if(--current == 0)
		{
			current = notes[index][++state] * rate;
		}
	}
	TA0CCTL0 = CCIE + soundOn; 		// update control register with current soundOn
}
ISR_VECTOR(sound_handler,".int09") // declare interrupt vector

// +++++++++++++++++++++++++++
// Button input System
// Button toggles the state of the sound (on or off)
// action will be interrupt driven on a downgoing signal on the pin
// no debouncing (to see how this goes)

void init_button(){
// All GPIO's are already inputs if we are coming in after a reset
	P1OUT |= START; // pullup
	P1REN |= START; // enable resistor
	P1IES |= START; // set for 1->0 transition
	P1IFG &= ~START;// clear interrupt flag
	P1IE  |= START; // enable interrupt

	P1OUT |= SPEEDUP; // pullup
	P1REN |= SPEEDUP; // enable resistor
	P1IES |= SPEEDUP; // set for 1->0 transition
	P1IFG &= ~SPEEDUP;// clear interrupt flag
	P1IE  |= SPEEDUP; // enable interrupt

	P1OUT |= SLOWDOWN; // pullup
	P1REN |= SLOWDOWN; // enable resistor
	P1IES |= SLOWDOWN; // set for 1->0 transition
	P1IFG &= ~SLOWDOWN;// clear interrupt flag
	P1IE  |= SLOWDOWN; // enable interrupt

	P1OUT |= REPLAY; // pullup
	P1REN |= REPLAY; // enable resistor
	P1IES |= REPLAY; // set for 1->0 transition
	P1IFG &= ~REPLAY;// clear interrupt flag
	P1IE  |= REPLAY; // enable interrupt

	P1OUT |= SELECT; // pullup
	P1REN |= SELECT; // enable resistor
	P1IES |= SELECT; // set for 1->0 transition
	P1IFG &= ~SELECT;// clear interrupt flag
	P1IE  |= SELECT; // enable interrupt
}
void interrupt button_handler(){
// check that this is the correct interrupt
// (if not, it is an error, but there is no error handler)
	if (P1IFG & START){
		P1IFG &= ~START; // reset the interrupt flag
		soundOn ^= OUTMOD_4; // flip bit for outmod of sound
	}
	if (P1IFG & SPEEDUP){
		P1IFG &= ~SPEEDUP; // reset the interrupt flag
		speedup ^= OUTMOD_4; // flip bit for outmod of sound
	}
	if (P1IFG & SLOWDOWN){
		P1IFG &= ~SLOWDOWN; // reset the interrupt flag
		slowdown ^= OUTMOD_4; // flip bit for outmod of sound
	}
	if (P1IFG & REPLAY){
		P1IFG &= ~REPLAY; // reset the interrupt flag
		replay ^= OUTMOD_4; // flip bit for outmod of sound
	}
	if (P1IFG & SELECT){
		P1IFG &= ~SELECT; // reset the interrupt flag
		select ^= OUTMOD_4; //
	}
}
ISR_VECTOR(button_handler,".int02") // declare interrupt vector
// +++++++++++++++++++++++++++
