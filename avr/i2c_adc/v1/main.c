/*********************************************************************
* ADC-TWI-Slave (TWI via USI)
*
* Uwe Berger; 2012
*
**********************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>

#include "USI_TWI_Slave.h"

// TWI-Slave-Adresse
#define TWI_SLAVE_ADDR	0x12				

   
//*********************************************************************
//*********************************************************************
int main(void)
{
	uint8_t adc_adr, temp;
  
	// TWI initialisieren und Slave-Adresse setzen
	USI_TWI_Slave_Initialise( TWI_SLAVE_ADDR );
	// Vcc als Referenz; Kanalwahl kommt spaeter
	ADMUX = 0;	
	// digitale Inputregister fuer die beiden ADC-Eingaenge abwaehlen
	DIDR0 |= (1<<ADC3D) | (1<<ADC2D);
	//Interrupt einschalten
	sei();
	// Endlos-Loop
	for(;;)	{
		//*** TWI-Verarbeitung
		if(USI_TWI_Data_In_Receive_Buffer()) {
			// ein Byte vom Master empfangen 
			adc_adr = USI_TWI_Receive_Byte();
			// ADC-Kanal auswaehlen
			switch (adc_adr) {
				case 0: {
					ADMUX |= (1<<MUX1);
					break;
				}
				case 1: {
					ADMUX |= (1<<MUX1) | (1<<MUX0);
					break;
				}
			}
			// ADC einschalten; ADC-Vorteiler auf 8
			ADCSRA = (1<<ADEN) | (1<<ADPS0) | (1<<ADPS1);
			// Dummy-Messung
			ADCSRA |= (1<<ADSC);              
			while (ADCSRA & (1<<ADSC));
			temp = ADCL;
			temp = ADCH;
			// eigentliche Messung
			ADCSRA |= (1<<ADSC);  
			while (ADCSRA & (1<<ADSC));
			// ADC wieder ausschalten
			ADCSRA=0;
			// Ergebnis via I2C senden
			USI_TWI_Transmit_Byte(ADCL);
			USI_TWI_Transmit_Byte(ADCH);	
		}
	}
}

