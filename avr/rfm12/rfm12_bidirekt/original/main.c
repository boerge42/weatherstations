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

		if (rx_data_in_buffer())							// Daten empfangen?
		{	unsigned char count, i;
			count=rx_data(data);
			for (i=0; i<count; i++)			// Empfangs Beispiel
				uart_putc(data[i]);
		}

		if (uart_data())					// Sende Beispiel 1: UART Daten senden
		{	data[0]=uart_getchar();
			uart_putc(tx_data(data,1));		// 1Byte Daten senden, Rückantwort ob Daten zugestellt wurden (0 = OK, 1 = Fehlgeschlagen)
		}
		
		if (secflag)										// 1Hz
		{	secflag=0;
			if (!--tdiv)					// Sende Beispiel 2: Daten im 30s Takt senden
			{	tdiv=30;
				data[0]=123;
				tx_data(data,1);			// 1Byte Daten senden
			}
		}
	}
}





