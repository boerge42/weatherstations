
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay_basic.h>
#include "usiTwiSlave.h"

#define I2C_SLAVE_ADDR  0x13


//*********************************************************************
uint8_t adcRead(void) {
	uint8_t t;
	// ADC-Ergebnis linksbuendig
	ADMUX |= 1 << ADLAR;
	// ADC einschalten; ADC-Vorteiler auf 8
	ADCSRA |= (1<<ADPS0) | (1<<ADPS1);
	ADCSRA |= 1 << ADEN;
	// Dummy-Messung	
	ADCSRA |= 1 << ADSC;
	while (ADCSRA & (1 << ADSC));
	t = ADCH;
	// eigentliche Messung
	ADCSRA |= 1 << ADSC;
	while (ADCSRA & (1 << ADSC));
	ADCSRA = 0;
	// Ergebnis in ADCH
	return ADCH;
}

//*********************************************************************
// A callback triggered when the i2c master attempts to read from a register.
uint8_t i2cReadFromRegister(uint8_t reg)
{
	switch (reg)
	{
		case 0: {
			ADMUX = (1<<MUX1);
			return adcRead();
		}
		
		case 1: {
			ADMUX = (1<<MUX1) | (1<<MUX0);
			return adcRead();
		}
		
		default:
			return 0xff;
	}
}

//*********************************************************************
// A callback triggered when the i2c master attempts to write to a register.
void i2cWriteToRegister(uint8_t reg, uint8_t value){
	
}

//*********************************************************************
//*********************************************************************
//*********************************************************************
int main(void) {
	
	// I2C-Adresse, Callback-Funktionen
	usiTwiSlaveInit(I2C_SLAVE_ADDR, i2cReadFromRegister, i2cWriteToRegister);
	sei();
	
	while (1) {
	}
}
