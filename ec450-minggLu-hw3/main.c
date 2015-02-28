/***************************************************************************************
 * ec450 Minying Lu hw3 
 ***************************************************************************************/

#include <msp430g2553.h>

// Bit masks for port 1
#define RED 0x01
#define GREEN 0x40
#define BUTTON 0x08

// Global state variables
volatile unsigned char last_button; // the state of the button bit at the last interrupt
volatile unsigned int reset;
volatile unsigned int mode;
volatile unsigned int interval;
volatile unsigned int record[60];
volatile unsigned int index;

void main(void) {
	  // setup the watchdog timer as an interval timer
	  WDTCTL =(WDTPW + // (bits 15-8) password
	                   // bit 7=0 => watchdog timer on
	                   // bit 6=0 => NMI on rising edge (not used here)
	                   // bit 5=0 => RST/NMI pin does a reset (not used here)
	           WDTTMSEL + // (bit 4) select interval timer mode
	           WDTCNTCL +  // (bit 3) clear watchdog timer counter
	  		          0 // bit 2=0 => SMCLK is the source
	  		          +1 // bits 1-0 = 01 => source/8K
	  		   );
	  IE1 |= WDTIE;		// enable the WDT interrupt (in the system interrupt register IE1)

	  P1DIR |= RED+GREEN;					// Set RED and GREEN to output direction
	  P1DIR &= ~BUTTON;						// make sure BUTTON bit is an input

	  P1OUT &= ~GREEN; 		// Green off
	  P1OUT |= RED;  		// Red on

	  P1DIR |= 0x01;					// Set P1.0 to output direction

	  P1OUT |= BUTTON;
	  P1REN |= BUTTON;						// Activate pullup resistors on Button Pin

	  mode = 0;				//set the beginning state to 0
	  _bis_SR_register(GIE+LPM0_bits);  // enable interrupts and also turn the CPU off!
}

// ===== Watchdog Timer Interrupt Handler =====
// This event handler is called to handle the watchdog timer interrupt,
//    which is occurring regularly at intervals of about 8K/1.1MHz ~= 7.4ms.

interrupt void WDT_interval_handler(){
  	unsigned char b;
  	b = !(P1IN & BUTTON);  				// BUTTON input
  	if(mode == 0){						// initialization mode
  		if(last_button ^ b == 1){		// when button is pressed down the first time set to record mode
  			reset = 0;
  			index = 0;
  			interval = 0;
  			P1OUT |= GREEN;
  			last_button = b;
  			mode = 1;
  		}
  	}

  	// mode 1 is recording mode
	if(mode == 1){
		interval++;
		P1OUT |= RED;					// turn on red to indicate record mode
		if(last_button ^ b == 1 && index < 59){
			record[index] = interval;
			interval = 0;
			P1OUT ^= GREEN;
			last_button = b;
			index++;
		}
		if(index >= 59){
			record[index] = interval;
			interval = record[reset];
			P1OUT |= GREEN;
			index++;
			mode = 2;
		}
		if(interval >= 300 && index > 0 && b == 0){
			record[index] = interval;
			interval = record[reset];
			P1OUT |= GREEN;
			index++;
			mode = 2;
		}
	}

	// mode 2 is play back mode
	if(mode == 2 && reset < index){
		interval--;
		P1OUT &= ~RED;						// turn off red light to indicate play back mode
		if(interval == 0){					// flash(toggle) green LED every time interval reaches 0
			P1OUT ^= GREEN;
			interval = record[++reset];
		}
		if(reset == index || reset >= 59){	// when finish replay mode set back to mode 0
			mode = 0;
			P1OUT |= RED;
			P1OUT &= ~GREEN;
		}
	}
}
// DECLARE function WDT_interval_handler as handler for interrupt 10
// using a macro defined in the msp430g2553.h include file
ISR_VECTOR(WDT_interval_handler, ".int10")
