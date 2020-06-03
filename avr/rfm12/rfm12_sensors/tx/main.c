/****************************************************************************
 *             Sender Wetter-Funkbruecke
 *             =========================
 *					Uwe Berger; 2013
 *
 * Hardware
 * ========
 * -Standard ATMega8 (AVcc an Vcc; F_CPU (MHz)
 * -RFM12-Modul: Schaltung siehe http://www.mikrocontroller.net/topic/71682
 * -Hardware-TWI: siehe entsprechendes Atmel-Datenblatt zur MCU
 * -diverse Sensoren...
 *
 * Software
 * ========
 *
 * TWI-Master-Lib:
 * ---------------
 * -Peter Fleury (http://homepage.hispeed.ch/peterfleury/avr-software.html#libs)
 * 
 * 
 * RFM12-Lib:
 * ----------
 * -Benedikt K. (http://www.mikrocontroller.net/topic/71682)
 * 
 * 
 * SHT11-LIB
 * ---------
 * -A.K./Thorsten S. (http://www.mikrocontroller.net/topic/145736)
 * 
 * 
 * allgemeiner Programmablauf:
 * ---------------------------
 * -es werden zyklisch die verschiedensten angeschlossenen Sensoren 
 *  abgefragt und die ermittelten Werte in der Struktur msg abgelegt
 * -Messung von Vcc mit Vbg als Referenz; dieser Wert landet ebenfalls 
 *  in msg
 *  (http://www.mikrocontroller.net/articles/Pollin_Funk-AVR-Evaluationsboard)
 * -der Inhalt von msg wird zyklisch via Funk gesendet
 * -in den "Pausen" wird die MCU und das RFM12-Modul in den Sleep-Modus 
 *  geschickt, um Strom zu sparen; das Wecken des Systems erfolgt ueber 
 *  den WakeUp-Timer des RFM12-Modul 
 *  (http://www.mikrocontroller.net/topic/241964)
 * 
 * 
 * ---------
 * Have fun! 
 *
 ***************************************************************************/

#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>

#include "i2cmaster.h"
#include "sht11.h"


#define DEBUG_LED		1			// LED leuchtet waehrend Messen/Senden

#define RF_BAUDRATE		15000		// Baudrate des RFM12

#define ADDR_LM75 		0x92		// I2C-Addr. LM75
#define ADDR_TSL45315	0x52		// I2C-Addr, TSL45315


#if DEBUG_LED
#define LED_DDR  		DDRC		// LED-Pin...
#define LED_PORT 		PORTC
#define LED_PIN  		PC3
#endif

#include "rf12.h"

#define RFM12_IRQ_DDR	DDRD		// INT0-Pin fuer RFM12-WakeUp
#define RFM12_IRQ_PORT	PORTD
#define RFM12_IRQ		(1<<PD2)

#define ADC_DUMMY_READS	10			// ein paar ADC-Parameter...
#define ADC_HOT_READS	20
#define V_BG			1230UL		// interner BandGap eigentlich 1,3V...?

// Funkmessage...
struct msg_t {					// Offset
	uint16_t counter;			// --> 0
	uint16_t vcc;				// --> 2
	uint32_t brightness;		// --> 4
	uint16_t sht15_humidity;	// --> 8
	uint16_t sht15_temperature;	// --> 10
	uint16_t tmp36;				// --> 12
} msg;

//*************************************************
ISR(INT0_vect) 
{
#if DEBUG_LED
	// LED toggeln
	LED_PORT ^= (1 << LED_PIN);
#endif
}

//*************************************************
void long_delay(uint16_t ms) 
{
    for (; ms>0; ms--) _delay_ms(1);
}

//*************************************************
void rf12_sleep_with_wakeup(void) 
{
	//	
	// Wake-Up Timer 
	// 1 1 1 r4 r3 r2 r1 r0 m7 m6 m5 m4 m3 m2 m1 m0
	// Twake-up = M * 2R [ms]
	//
//	rf12_trans(0xE730);  // ca. 6s
	rf12_trans(0xE8EA);  // ca. 60s
	//
	// Power Management
	// 1 0 0 0 0 0 1 0 er ebb et es ex eb ew dc
	// er  Enables the whole receiver chain (RF front end, baseband, 
	//     synthesizer, oscillator)
	// ebb The receiver baseband circuit can be separately switched on 
	//     (Baseband)
	// et  Switches on the PLL, the power amplifier, and starts the 
	//     transmission (If TX register is enabled) (Power amplifier, 
	//     synthesizer, oscillator)
	// es  Turns on the synthesizer (Synthesizer)
	// ex  Turns on the crystal oscillator (Crystal oscillator)
	// eb  Enables the low battery detector (Low battery detector)
	// ew  Enables the wake-up timer (Wake-up timer)
	// dc  Disables the clock output (pin 8) (Clock output buffer)
	//
//	rf12_trans(0x820B);
	rf12_trans(0x8203);
	// Status Read Command...
	rf12_trans(0x0000);
}

//*************************************************
void radio_init(void) 
{
	rf12_init();										
	rf12_config(RF_BAUDRATE, RF12FREQ(433.92), 0, QUIET);
	rf12_rxmode();		
}

//*************************************************
uint8_t tsl45315_init(uint8_t addr)
{
		//
		// Control-Register (0x00)
		// 0x00 --> power down
		// 0x01 --> reserved
		// 0x02 --> run a single ADC cycle and power down
		// 0x03 --> normal operation
        if (i2c_start(addr+I2C_WRITE)) return 1;
        i2c_write(0x80);        // Adresse Control-Register
        i2c_write(0x03);        // Wert --> normal operation mode
        i2c_stop();
        //
        // Config-Register (0x01)
		// Bit3 --> PSAVESKIP
		// Bit1:0
		//   0x00 --> 400ms
		//   0x01 --> 200ms
		//   0x02 --> 100ms
        if (i2c_start(addr+I2C_WRITE)) return 1;
        i2c_write(0x81);        // Adresse Config-Register
        i2c_write(0x01);        // Wert --> sensor time = 200ms
        i2c_stop();
        return 0;
}

//*************************************************
uint32_t tsl45315_read(uint8_t addr)
{
        uint16_t l, h;
        // Leseadresse (Low-Byte) senden
        if (i2c_start(addr+I2C_WRITE)) return 0;
        i2c_write(0x84);
        i2c_stop();
        // 2 Byte lesen (Low, Hight)
        if (i2c_start(addr+I2C_READ)) return 0;
        l = i2c_readAck();
        h = i2c_readNak();
        i2c_stop();
        //
		// 400ms --> 1x
		// 200ms --> 2x
		// 100ms --> 4x
        return (2 * (h * 256 + l));
}

//*************************************************
uint16_t adc_read(uint8_t val_admux, uint8_t dummy_reads, uint8_t hot_reads)
{
	uint32_t adcval;
	uint8_t i;
	ADMUX = val_admux;
	//
	//	ADC optimal zwischen 50 kHz und 200 kHz...
    //   F_CPU/200000 = min. Vorteiler
    //   F_CPU/50000  = max. Vorteiler
	// Vorteiler (64) und ADC einschalten
	ADCSRA = (1<<ADEN) | (1<<ADPS1) | (1<<ADPS2);
	// sicherheitshalber ein paar Dummy-Messungen
	for(i=0; i < dummy_reads; i++) {
		ADCSRA |= (1<<ADSC);
    	while (ADCSRA&(1<<ADSC)) {}
		adcval = ADCW;
	}
	// reale Messungen
	adcval = 0;
	for(i=0; i < hot_reads; i++) {
		ADCSRA |= (1<<ADSC);
		while (ADCSRA&(1<<ADSC)) {}
		adcval += ADCW;
	}    
	// ADC ausschalten
    ADCSRA &= ~(1<<ADEN);
    // Mittelwert zurueckgeben
	return adcval/hot_reads;
}

//*************************************************
//*************************************************
//*************************************************
int main(void) 
{
	uint32_t val32;
	
	// Init MCU
#if DEBUG_LED
	LED_DDR   = (1 << LED_PIN);	
#endif

	RFM12_IRQ_PORT &= ~RFM12_IRQ;
	MCUCR |= (0<<ISC01) | (0<<ISC00);

	sei();
	
	i2c_init();
	sht11_init();	

	msg.counter = 1;

	while (1) {

		// RFM12-Modul initialisieren...
		radio_init();
		
		// interne Empfangsroutine, muss periodisch aufgerufen werden
		check_rx_packet();	

		//
		// zu versendene Werte einsammeln und versenden
		//
		
		// Vcc ermitteln (Ermittlung indirekt via Bandgab-Referenz)
		val32 = adc_read((1<<REFS0)|(1<<MUX3)|(1<<MUX2)|(1<<MUX1)|(0<<MUX0), ADC_DUMMY_READS, ADC_HOT_READS);
		msg.vcc = (V_BG*1024)/val32;

		// Helligkeit (TSL45315)
		tsl45315_init(ADDR_TSL45315);
		long_delay(250);
		msg.brightness = tsl45315_read(ADDR_TSL45315);
		
		// SHT15
		sht11_start_measure();
		while( !sht11_measure_finish() );
		msg.sht15_humidity		= sht11_get_hum();
		msg.sht15_temperature	= sht11_get_tmp();
		
		// TMP36 an ADC0; Referenz Avcc...
		val32 = adc_read((1<<REFS0), 1, 32);
		msg.tmp36 = (val32*msg.vcc)/1024;
		
		// Message via RFM12 versenden
		tx_data((unsigned char *)&msg, sizeof(msg));
			
		// Message-Zaehler erhoehen
		msg.counter++;

		// ...und alles schlafen schicken 
		// (Aufwachen via RFM12 --> INT0)
		rf12_sleep_with_wakeup();
		GICR |= (1 << INT0);
		set_sleep_mode(SLEEP_MODE_PWR_DOWN);
		sleep_mode();                  
		GICR &= ~(1 << INT0); 
		// wegen TWI-Bug nach Sleep-Mode ein TWI-Reset...
		// (http://www.mikrocontroller.net/articles/AVR_TWI#Bekannte_Bugs)
		TWCR &= ~(TWSTO+TWEN);
		TWCR |= TWEN;

	}
}
