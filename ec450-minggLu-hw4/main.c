// the following line loads definitions for the
#include  <msp430g2553.h>

// mask to select P1.1 as the output pin
#define OUTPINMASK 0x2

// ===== Main Program (System Reset Handler) =====
void main(void)
{
	// from MSP430 Family Manual page 29 DOC frequency table
	// know that DOC frequency(11, 3) gives 4.25MHz
	// set DOC frequency(11, 3)
	DCOCTL &= ~BIT7;			// set DOC bits(7-5) to 101 (3) 
	DCOCTL |= BIT5 + BIT6;

	BCSCTL1 &= ~(BIT2);			// set RSEL bits(3-0) to 1011 (11)
	BCSCTL1 |= BIT0 + BIT1 + BIT3;

  // setup the watchdog timer as an interval timer
  WDTCTL =(WDTPW +	// (bits 15-8) password
                        // bit 7=0 => watchdog timer on
                        // bit 6=0 => NMI on rising edge (not used here)
                        // bit 5=0 => RST/NMI pin does a reset (not used here)
           WDTTMSEL +  // (bit 4) select interval timer mode
           WDTCNTCL + 	// (bit 3) clear watchdog timer counter
  		                // bit 2=0 => SMCLK is the source
  		  	2	// bits 1-0 = 10 => source/512 .
  		   );
  IE1 |= WDTIE;		// enable the WDT interrupt (in the system interrupt register IE1)

  // initialize the I/O port.
  P1DIR = OUTPINMASK;     // Set direction for output pinoutput,

 // in the CPU status register, set bits:
 //    GIE == CPU general interrupt enable
 //    LPM0_bits == bit to set for CPUOFF (which is low power mode 0)

 _bis_SR_register(GIE+LPM0_bits);  // after execution of this instruction, the CPU is off!
}

// ===== Watchdog Timer Interrupt Handler =====
// This event handler is called to handle the watchdog timer interrupt,
//    which is occurring regularly at intervals of 32K/8MHz ~= 4ms.

interrupt void WDT_interval_handler(){
	P1OUT ^= OUTPINMASK; // toggle the output pin at regular intervals
}

// DECLARE WDT_interval_handler as handler for interrupt 10
ISR_VECTOR(WDT_interval_handler, ".int10")
