/*
 * lcd.h
 *
 * Modified on: April 8, 2014
 *      Author: Roscoe Giles
 */

/*
 * The LCD panel is connected to the low 6 pins of 1 port (Port 2 below) and
 * 1 pin of another port (P1.0) below.
 * NOTE: remove the jumper for the red LED on P1.0!!
 *
 */

#ifndef LCD_H_
#define LCD_H_


/*************************************************
 * -----------------------------------
 * Functions visible to other programs
 * -----------------------------------
 * LCD_setup(): initialize the output pins and timer TA1
 * LCD_init():  initialize the LCD panel state (call AFTER GIE enabled)
 * -------------------------
 * LCD_put(int value),
 * start_LCD_put(int value):
 *
 * These both send a 9-bit value to the LCD.
 * LCD_put waits for the program's state machine to finish operating before it returns.
 * start_LCD_put does not wait.
 *
 * NOTE: the LCD takes time to process instructions -- see the LCD documentation.
 * You may need some explicit delays after each operation.
 *
 * ------------------------------
 * These all send to the DATA register of the LCD only,
 * and DO NOT wait!
 * start_LCD_send_char(char c);
 * start_LCD_send_buffer(char *buffer, int count);
 * start_LCD_send_string(char *s);
 *
 * ------------------------------
 * LCD_send_string(char *s):
 * sends characters of a string to the data register using a series of calls to LCD_put.
 * *** this is just for demo purposes, since it busy-waits!
 * ------------------------------
 * delay(unsigned int ticks):
 * uses the TA1 state machine to delay for a period of time (with a busy wait!)
 * intended for initialization and some time dependent changes of LCD
 * ------------------------------
 * LCD_SM_busy(): returns true if the LCD state machines are busy
 *
 */

void LCD_put(int value); // send a value to the LCD including RS bit
                         // value is 9 bits: (RS followed by 8 data bits).
void LCD_setup(void); // initialize Timer A1 and pins...
void LCD_init(void); // initialize LCD for communications (after GIE is enable!)
void LCD_send_string(char *); // sends a string to the data register
void delay(unsigned int);
int LCD_SM_busy();


#endif /* LCD_H_ */
