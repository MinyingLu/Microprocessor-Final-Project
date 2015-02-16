/***************************************************************************************
***********************WRITE-UP*****************************
************************************************************
Collaborator: Philip Zhang
use variable sos to keep track of the state of the system between WDT interupts.
use LED1 (P1.0OUT) to blink

 ***************************************************************************************/

#include <msp430g2553.h>

volatile unsigned int blink_interval;  // number of WDT interrupts per blink of LED
volatile unsigned int blink_counter;   // down counter for interrupt handler
volatile unsigned int sos;
int main(void) {
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

	P1DIR |= 0x01;					// Set P1.0 to output direction

	// initialize the state variables
	blink_interval=67;                // the number of WDT interrupts per toggle of P1.0
									// call this 1 unit
	blink_counter=blink_interval;     // initialize the counter
	sos = 0;
	P1OUT &= 0xFE;
	_bis_SR_register(GIE+LPM0_bits);  // enable interrupts and also turn the CPU off!
}

// ===== Watchdog Timer Interrupt Handler =====
// This event handler is called to handle the watchdog timer interrupt,
//    which is occurring regularly at intervals of about 8K/1.1MHz ~= 7.4ms.

interrupt void WDT_interval_handler(){
	--blink_counter;
	//short flashes, with 1 unit interval
	if (blink_counter == 0 && (sos >= 0 && sos <= 5)){         
		P1OUT ^= 1;						// toggle LED on P1.0
		blink_counter=blink_interval;	// reset the down counter
		sos++;
	}
	//break between group of flashes, 3 units
	else if (blink_counter == 0 && sos == 6){
		blink_counter = 3 * blink_interval;
		sos++;
	}
	//3 units long flashes
	else if(blink_counter == 0 && (sos >= 7 && sos <= 11) && (sos % 2 == 1)){
		P1OUT ^= 1;
		blink_counter = 3 * blink_interval;
		sos++;
	}
	//with 1 unit break
	else if (blink_counter == 0 && (sos >= 7 && sos <= 11) && (sos % 2 == 0)){
		P1OUT ^= 1;
		blink_counter = blink_interval;
		sos++;
	}
	//the second 3 units break
	else if (blink_counter == 0 && sos == 12){
		P1OUT = 0;
		blink_counter = 3 * blink_interval;
		sos++;
	}
	//the second 1 unit flashes and breaks
	else if (blink_counter == 0 && (sos >= 13 && sos <= 17)){          // decrement the counter and act only if it has reached 0
		P1OUT ^= 1;                   // toggle LED on P1.0
		blink_counter=blink_interval; // reset the down counter
		sos++;
	}
	//7 units break between messages. 
	else if (blink_counter == 0 && sos == 18){
		P1OUT = 0;
		sos = 0;
		blink_counter = 7 * blink_interval;
	}
}

// DECLARE function WDT_interval_handler as handler for interrupt 10
// using a macro defined in the msp430g2553.h include file
ISR_VECTOR(WDT_interval_handler, ".int10")
