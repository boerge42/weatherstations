/****************************************************************************
 *         Empfaenger Wetter-Funkbruecke als TWI-Slave
 *         ===========================================
 *                     Uwe Berger; 2013
 *
 * TWI-Slave-Lib:
 * --------------
 * -Martin Junghans
 * -http://www.jtronics.de/avr-projekte/library-i2c-twi-slave.html
 * 
 * RFM12-Lib:
 * ----------
 * -Benedikt K.
 * -http://www.mikrocontroller.net/topic/71682
 * 
 * 
 * Hardware
 * --------
 * -RFM12-Modul: Schaltung siehe http://www.mikrocontroller.net/topic/71682
 * -Hardware-TWI: siehe entsprechendes Atmel-Datenblatt zur MCU
 * 
 * Software
 * --------
 * -Empfang von Daten von einem entsprechenden Sender via Funk
 * -Ablage der empfangenen Datenbytes in gesendeter Reihenfolge in einem
 *  Puffer
 * -Pufferinhalt kann via TWI (I2C) abgefragt werden (Adressierung der
 *  entsprechenden Pufferadresse z.B. analog eine I2C-EEPROM...) 
 * 
 * 
 * ---------
 * Have fun! 
 *
 ***************************************************************************/

#include <avr/interrupt.h>
#include "rf12.h"
#include "twislave.h"

#define RF_BAUDRATE		15000		// Baudrate des RFM12

unsigned char data[128];

#define I2C_SLAVE_ADRESSE 0x50      // i2cdetect ermittelt 0x28...


//****************************************************
//****************************************************
//****************************************************
int main(void)
{	
	PORTB =	0b00000001;		//1
	PORTD =	0b00011111;		//31
	DDRC  =	0b00111111;		//63
	DDRD  =	0b11101110;		//238

	init_twi_slave(I2C_SLAVE_ADRESSE);

	sei();
	
	rf12_init();											// RF12 + IO Ports initialisieren
	rf12_config(RF_BAUDRATE, RF12FREQ(433.92), 0, QUIET);	// Baudrate, Frequenz, Leistung (0=max, 7=min), Umgebungsbedingungen (QUIET, NORMAL, NOISY)
	rf12_rxmode();											// Empfang starten

	for (;;) {	
		check_rx_packet();									// interne Empfangsroutine, muss periodisch aufgerufen werden

		if (rx_data_in_buffer()) {							// RFM12-Daten empfangen?
			unsigned char count, i;
			count=rx_data(data);
			for (i=0; i<count; i++) {						// RFM12-Daten in TWI-Puffer schreiben
				txbuffer[i] = data[i];
			}
		}
	}
}





