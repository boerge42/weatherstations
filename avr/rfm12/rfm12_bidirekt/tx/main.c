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
#include "portbits.h"
#include "global.h"
#include "rf12.h"
#include "i2cmaster.h"
#include <util/delay.h>

#define RF_BAUDRATE		15000		// Baudrate des RFM12

#define LED_DDR  DDRD						// Hardwarekonfiguration
#define LED_PORT PORTD
#define LED_PIN  PD4

#define ADDR_LM75 	0x92

char data[128];
uint16_t counter = 0;

unsigned char h_byte;	// Hight-Byte Temperatur-Registers LM75
unsigned char l_byte;	// Low-Byte Temperatur-Registers LM75

struct msg_t {
	uint16_t 		counter;
	//unsigned char 	lm75_h_byte;
	//unsigned char 	lm75_l_byte;
	uint16_t		lm75;
	uint16_t 		ds1820;
	uint16_t		hih4030;
	uint16_t		adc;
} msg;


void lm75_read(uint8_t addr)
{
	i2c_start(addr+I2C_READ);
	h_byte = i2c_readAck();
	l_byte = i2c_readNak();
	i2c_stop();
}



int main(void) {
	
	LED_DDR   = (1 << LED_PIN);				// LED-Pin als Ausgang schalten
	LED_PORT &= ~(1 << LED_PIN);			// LED ausschalten

	sei();
	
	i2c_init();					// TWI initialisieren
	
	
	rf12_init();											// RF12 + IO Ports initialisieren
	rf12_config(RF_BAUDRATE, RF12FREQ(433.92), 0, QUIET);	// Baudrate, Frequenz, Leistung (0=max, 7=min), Umgebungsbedingungen (QUIET, NORMAL, NOISY)
	rf12_rxmode();											// Empfang starten

	counter = 0;


	for (;;) {
		check_rx_packet();									// interne Empfangsroutine, muss periodisch aufgerufen werden
		
		if (secflag) {										// 1Hz
			secflag=0;
				LED_PORT ^= (1 << LED_PIN);
				
				msg.counter = counter;

				// LM75
				//lm75_read(ADDR_LM75);
				//msg.lm75 = l_byte * 256 + h_byte;
				msg.lm75 = 4;
				
				// HIH4030
				msg.hih4030 = 1;
				
				// ADC
				msg.adc = 2;
				
				// DS18S20
				msg.ds1820 = 3;
				
				tx_data((unsigned char *)&msg, sizeof(msg));

				counter++;
		}
	}
}





