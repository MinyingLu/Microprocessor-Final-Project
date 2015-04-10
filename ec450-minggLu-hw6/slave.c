
#include "msp430g2553.h"

 /* declarations of functions defined later */
 void init_spi(void);
 void init_wdt(void);

// Global variables and parameters (all volatilel to maintain for debugger)

volatile unsigned char data_received= 0; 	// most recent byte received

// Try for a fast send.  One transmission every 64 microseconds
// bitrate = 1 bit every 4 microseconds
#define ACTION_INTERVAL 1
#define BIT_RATE_DIVISOR 32

#define TA0_BIT 0x02

//Bit positions in P1 for SPI
#define SPI_CLK 0x20
#define SPI_SOMI 0x40
#define SPI_SIMO 0x80

// calculate the lo and hi bytes of the bit rate divisor
#define BRLO (BIT_RATE_DIVISOR &  0xFF)
#define BRHI (BIT_RATE_DIVISOR / 0x100)
//----------------------------------------------------------------

// ======== Receive interrupt Handler for UCB0 ==========

void interrupt spi_rx_handler(){
	data_received=UCB0RXBUF; // copy data to global variable
	TA0CCR0 = (2100*data_received)/250 + 400  ;
	IFG2 &= ~UCB0RXIFG;		 // clear UCB0 RX flag

}
ISR_VECTOR(spi_rx_handler, ".int07")


void init_spi(){
	UCB0CTL1 = UCSSEL_2+UCSWRST;  		// Reset state machine; SMCLK source;
	UCB0CTL0 = UCCKPH					// Data capture on rising edge
			   							// read data while clock high
										// lsb first, 8 bit mode,
			   +UCMODE_0				// 3-pin SPI mode
			   +UCSYNC;					// sync mode (needed for SPI or I2C)
	UCB0BR0=BRLO;						// set divisor for bit rate
	UCB0BR1=BRHI;
	UCB0CTL1 &= ~UCSWRST;				// enable UCB0 (must do this before setting
										//              interrupt enable and flags)
	IFG2 &= ~UCB0RXIFG;					// clear UCB0 RX flag
	IE2 |= UCB0RXIE;					// enable UCB0 RX interrupt
	// Connect I/O pins to UCB0 SPI
	P1SEL =SPI_CLK+SPI_SOMI+SPI_SIMO;
	P1SEL2=SPI_CLK+SPI_SOMI+SPI_SIMO;
}

void init_timer(){              // initialization and start of timer
	TA0CTL |= TACLR;              // reset clock
	TA0CTL = TASSEL_2+ID_0+MC_1;  // clock source = SMCLK
	                            // clock divider=1
	                            // UP mode
	                            // timer A interrupt off
	TA0CCTL0=0; // compare mode, output 0, no interrupt enabled
	TA0CCR0 = 500; // in up mode TAR=0... TACCRO-1
	P1SEL|=TA0_BIT; // connect timer output to pin
	P1DIR|=TA0_BIT;
}

void main(){

	WDTCTL = WDTPW + WDTHOLD;       // Stop watchdog timer
	BCSCTL1 = CALBC1_8MHZ;			// 8Mhz calibration for clock
    DCOCTL  = CALDCO_8MHZ;

  	init_spi();
  	init_timer();
	TACCTL0 ^= OUTMOD_4;

 	_bis_SR_register(GIE+LPM0_bits);



}
