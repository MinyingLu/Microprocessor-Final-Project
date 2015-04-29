asm(" .length 10000");
asm(" .width 132");


#include <msp430g2553.h>
#include "lcd.h"

// include declarations for snprintf from the library & generic string stuff used in main
#include <string.h>
#include <stdio.h>

// string format for the second display iine.
#define HEX "R:%x G:%x B:%x     "
#define NUM "R%d G%d B%d   "
// constant data for initial printout.

// global string buffer variable for snprintf()
char hex[20]; // second output line buffer
char dec[30];
char msg[] = "The Hexdecimal: ";

#define RED_LED 0x02
#define GREEN_LED 0x04
#define BLUE_LED 0x20

#define ADC_INPUT_BIT_MASK 0x10
#define ADC_INCH INCH_4

#define BUTTON_BIT 0x08

#define MAX_MEASUREMENTS 5
#define CURRENT_DELAY 100

void init_adc(void);
void init_wdt(void);
void init_pins(void);
void init_button(void);

volatile double colourArray[] = {0,0,0};
volatile double whiteArray[] = {0,0,0};
volatile double blackArray[] = {0,0,0};
volatile double nomath[] = {0, 0, 0};

volatile unsigned int Current_LED = 0; //0 = red 1 = green 2 = blue
volatile unsigned int begin = 0;
volatile unsigned int Current_Target = 0;
volatile unsigned int Times_Measured = 0;
volatile unsigned int WDT_delay = 0;
volatile double grayDiff = 0;
volatile unsigned int measure = 0;
volatile unsigned char last_button;

void interrupt adc_handler(){
	 unsigned int i;
	 if(measure){
		 switch(Current_Target){
		 case 0:
			 whiteArray[Current_LED] += ADC10MEM;
			 Current_LED++;
			 measure = 0;
			 if(Current_LED == 3){
				 Current_LED = 0;
			 }

		 break;
		 case 1:
			 blackArray[Current_LED] += ADC10MEM;
			 Current_LED++;
			 measure = 0;
			 if(Current_LED == 3){
				 Current_LED = 0;
			 }

		 break;
		 case 2:
			 colourArray[Current_LED] += ADC10MEM;
			 Current_LED++;
			 measure = 0;
			 if(Current_LED == 3){
				 Current_LED = 0;
			 }

		 break;
		 }
		 if(Times_Measured == MAX_MEASUREMENTS){
			switch(Current_Target){
				 case 0:
					 for( i = 0 ; i < 3; i++){
						 whiteArray[i] = whiteArray[i]/MAX_MEASUREMENTS;
					 }
				 break;
				 case 1:
					 for( i = 0 ; i < 3; i++){
						 blackArray[i] = blackArray[i]/MAX_MEASUREMENTS;
					 }
				 break;
				 case 2:
					 for( i = 0 ; i < 3; i++){
						 colourArray[i] = colourArray[i]/MAX_MEASUREMENTS;
						 if (colourArray[i] < blackArray[i]){
							 colourArray[i] = 0;
						 }
						 else if(colourArray[i] > whiteArray[i]){
							 colourArray[i] = 255;
						 }
						 else{
							 grayDiff = whiteArray[i] - blackArray[i];
							 colourArray[i] = ((colourArray[i] - blackArray[i])/(grayDiff))*255;
						 }
					 }
				 break;
			}

			Times_Measured = 0;
			begin = 0;
			WDT_delay = CURRENT_DELAY;
			if(Current_Target < 2){
				Current_Target++;
			}
		}
	 }
}
ISR_VECTOR(adc_handler, ".int05")


 // ===== Watchdog Timer Interrupt Handler =====
interrupt void WDT_interval_handler(){
	ADC10CTL0 |= ADC10SC;  // trigger a conversion
	unsigned char b;
	unsigned char i;
	b= (P1IN & BUTTON_BIT);
	if (last_button && (b==0)){	//if the external button is pressed decrement and then jump back up to a tempo of 10
		begin = 1;
		WDT_delay = CURRENT_DELAY;
		if(Current_Target >= 2){
			for( i = 0 ; i < 3; i++){
				 colourArray[i] = 0;
			 }
			Current_Target = 2;
		}
	}

	if(WDT_delay == 0 && !begin){
		P1OUT |= RED_LED;
		P1OUT |= GREEN_LED;
		P1OUT |= BLUE_LED;
	}

	if(--WDT_delay == 0 && begin) {
		switch(Current_LED){
		case 0:
			P1OUT |= BLUE_LED;
			P1OUT |= GREEN_LED;
			P1OUT &= ~RED_LED;
			measure = 1;
		break;
		case 1:

			P1OUT |= RED_LED;
			P1OUT |= BLUE_LED;
			P1OUT &= ~GREEN_LED;
			measure = 1;
		break;
		case 2:
			P1OUT |= RED_LED;
			P1OUT |= GREEN_LED;
			P1OUT &= ~BLUE_LED;
			Times_Measured++;
			measure = 1;
		break;
		}

		WDT_delay = CURRENT_DELAY;

	}
	last_button=b;
}
ISR_VECTOR(WDT_interval_handler, ".int10")

// Initialization of the ADC
void init_adc(){
  ADC10CTL1= ADC_INCH	//input channel 4
 			  +SHS_0 //use ADC10SC bit to trigger sampling
 			  +ADC10DIV_4 // ADC10 clock/5
 			  +ADC10SSEL_0 // Clock Source=ADC10OSC
 			  +CONSEQ_0; // single channel, single conversion
 			  ;
  ADC10AE0=ADC_INPUT_BIT_MASK; // enable A4 analog input
  ADC10CTL0= SREF_0	//reference voltages are Vss and Vcc
 	          +ADC10SHT_3 //64 ADC10 Clocks for sample and hold time (slowest)
 	          +ADC10ON	//turn on ADC10
 	          +ENC		//enable (but not yet start) conversions
 	          +ADC10IE  //enable interrupts
 	          ;
}

void interrupt button_handler(){
	if (P1IFG & BUTTON_BIT){
		P1IFG &= ~BUTTON_BIT; // reset the interrupt flag
	}
}

ISR_VECTOR(button_handler,".int02")
 void init_wdt(){
	// setup the watchdog timer as an interval timer
  	WDTCTL =(WDTPW +		// (bits 15-8) password
     	                   	// bit 7=0 => watchdog timer on
       	                 	// bit 6=0 => NMI on rising edge (not used here)
                        	// bit 5=0 => RST/NMI pin does a reset (not used here)
           	 WDTTMSEL +     // (bit 4) select interval timer mode
  		     WDTCNTCL  		// (bit 3) clear watchdog timer counter
  		                	// bit 2=0 => SMCLK is the source
  		                	// bits 1-0 = 00 => source/32K
 			 );
     IE1 |= WDTIE;			// enable the WDT interrupt (in the system interrupt register IE1)
 }

void init_pins(){
		P1DIR |= RED_LED;					// Set P1.0 to output direction
		P1DIR |= GREEN_LED;
		P1DIR |= BLUE_LED;

		P1OUT |= RED_LED;
		P1OUT |= GREEN_LED;
		P1OUT |= BLUE_LED;

}

void init_button(){
// All GPIO's are already inputs if we are coming in after a reset
	P1OUT |= BUTTON_BIT; // pullup
	P1REN |= BUTTON_BIT; // enable resistor
	P1IES |= BUTTON_BIT; // set for 1->0 transition
	P1IFG &= ~BUTTON_BIT;// clear interrupt flag
	P1IE  |= BUTTON_BIT; // enable interrupt

}

/*
 * main.c
 */
void main() {
	init_pins();
	init_button();
	init_adc();
	init_wdt();
	WDT_delay = CURRENT_DELAY;

 	BCSCTL1 = CALBC1_8MHZ;
	DCOCTL =  CALDCO_8MHZ;

	LCD_setup();
	_bis_SR_register(GIE);	// enable interrupts
	// initialize LCD and say hello a few times :-)
	LCD_init();

	LCD_send_string((char*)msg);

	while(1){
		LCD_put(0x80+40); // cursor to line 2, first position
		snprintf(hex, 20, HEX, (int)colourArray[1], (int)colourArray[2], (int)colourArray[0]);
		LCD_send_string(hex);
	}

	_bis_SR_register(LPM0_bits);

	//delay(0); // maximum delay



}
