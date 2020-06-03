/****************************************************************************
 *																			*
 *				universelle RFM12 Punkt zu Punkt Übertragung				*
 *																			*
 *								Version 1.0									*
 *																			*
 *								© by Benedikt								*
 *																			*
 *						Email:	benedikt83 ät gmx.net						*
 *																			*
 ****************************************************************************
 *																			*
 *	Die Software darf frei kopiert und verändert werden, solange sie nicht	*
 *	ohne meine Erlaubnis für kommerzielle Zwecke eingesetzt wird.			*
 *																			*
 ***************************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <stdlib.h>
#include <portbits.h>
#include "global.h"
#include "uart.h"
#include "rf12.h"
// #define F_CPU 16000000UL
#include <util/delay.h>

#define RF_BAUDRATE		15000		// Baudrate des RFM12

unsigned char data[128];
unsigned char tdiv=30;
unsigned char c;

uint8_t n2hex(uint8_t val) {
	if (val > 9){
		return val + 'A' - 10;
	} else {
		return val + '0';
	}
}


int main(void)
{	PORTB=1;
	PORTD=31;
	DDRC=63;
	DDRD=238;

	sei();
	
    uart_init(UART_BAUD_SELECT(19200, F_CPU));
	uart_puts("RFM12-Testprogramm...");

	
	rf12_init();											// RF12 + IO Ports initialisieren
	rf12_config(RF_BAUDRATE, RF12FREQ(433.92), 0, QUIET);	// Baudrate, Frequenz, Leistung (0=max, 7=min), Umgebungsbedingungen (QUIET, NORMAL, NOISY)
	rf12_rxmode();											// Empfang starten

	for (;;)
	{	check_rx_packet();									// interne Empfangsroutine, muss periodisch aufgerufen werden

		if (rx_data_in_buffer()) {							// Daten empfangen?
			unsigned char count, i;
			count=rx_data(data);
			for (i=0; i<count; i++) {			// Empfangs Beispiel
			
				//uart_putc(data[i]);
				
				uart_putc(n2hex(data[i] >> 4));
				uart_putc(n2hex(data[i] & 0x0f));				
				uart_putc(' ');
			}
			uart_puts("\n\r");
			
		}

		
	}
}





