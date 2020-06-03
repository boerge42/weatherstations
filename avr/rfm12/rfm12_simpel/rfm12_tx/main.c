#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <stdlib.h>
#include "global.h"
#include "uart.h"
#include "rf12.h"     

//#define F_CPU 8000000UL
#include <util/delay.h>

#define PRELOAD		59676					// Preload-Wert des Timers (0,5s zw. jedem Timer-Overflow)

#define LED_DDR  DDRD						// Hardwarekonfiguration
#define LED_PORT PORTD
#define LED_PIN  PD4


void send(void);
void receive(void);

int main(void)
{
	

	LED_DDR   = (1 << LED_PIN);				// LED-Pin als Ausgang schalten
	LED_PORT &= ~(1 << LED_PIN);			// LED ausschalten
	
	
    uart_init(UART_BAUD_SELECT(19200, F_CPU));
	rf12_init();					// ein paar Register setzen (z.B. CLK auf 10MHz)
	rf12_setfreq(RF12FREQ(433.92));	// Sende/Empfangsfrequenz auf 433,92MHz einstellen
	rf12_setbandwidth(4, 1, 4);		// 200kHz Bandbreite, -6dB Verstärkung, DRSSI threshold: -79dBm 
	rf12_setbaud(19200);			// 19200 baud
	rf12_setpower(0, 6);			// 1mW Ausgangangsleistung, 120kHz Frequenzshift

	sei();
	uart_puts_P("Sender laeuft !\n");

	for (;;)
	{	send();
		for (unsigned char i=0; i<100; i++)
			_delay_ms(10);
		//uart_puts_P("sende...\n");
		LED_PORT ^= (1 << LED_PIN);
	}
}

void receive(void)
{	unsigned char test[32];	
	rf12_rxdata(test,32);	
	for (unsigned char i=0; i<32; i++)
		uart_putc(test[i]);
}

void send(void)
//                        123456789012345678901234567 8 9012
{	unsigned char test[]="12345678901234567890123456789012";	
	rf12_txdata(test,32);
}

