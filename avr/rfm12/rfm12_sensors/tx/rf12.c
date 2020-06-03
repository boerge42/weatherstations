#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/crc16.h>
#include "portbits.h"
#include "global.h"
#include "rf12.h"
// #define F_CPU 10000000UL
#include <util/delay.h>

#define MAX_BUF			128			// Paket Größe in Bytes (maximal 250)
#define ANSWER_TIMEOUT	15			// Maximale Wartezeit auf die Bestätigung der Daten in ms (max 500)
#define RETRY			50			// Maximale Anzahl an Sendeversuchen

#define RF_PORT	PORTB				// SPI Port
#define RF_DDR	DDRB
#define RF_PIN	PINB

#define SDI		3					// MOSI Pinnummer
#define SCK		5					// SCK Pinnummer
#define CS		2					// CS Pinnummer
#define SDO		4					// MISO Pinnummer

#define MAX_BUF		128				// Maximale Paketgröße

#define LED_TX		PORTC_5			// TX LED
#define LED_RX		PORTC_3			// RX LED
#define LED_RETRANS	PORTC_4			// Retransmit LED
#define LED_ERR		PORTC_4			// Error LED
#define LED_POWER	PORTC_2			// Power LED


// #define USE_RSSI					// RSSI Messung über ADC
#define RSSIADC		7				// ADC Kanal an den ARSSI angeschlossen ist

#ifdef USE_RSSI
volatile unsigned short rssi;
#endif
volatile unsigned char delaycnt, secflag;
unsigned char rxbuf[MAX_BUF+1];		// Puffer für empfangene Daten
unsigned char flags, tx_cnt, rx_cnt, tx_id, tx_status, retrans_cnt;

#define NO_LED		unsigned char dummyledvar

// forwards
void rx_packet(void);


unsigned short rf12_trans(unsigned short wert)
{	CONVERTW val;
	val.w=wert;
	cbi(RF_PORT, CS);
	SPDR = val.b[1];
	while(!(SPSR & (1<<SPIF)));
	val.b[1]=SPDR;
	SPDR = val.b[0];
	while(!(SPSR & (1<<SPIF)));
	val.b[0]=SPDR;
	sbi(RF_PORT, CS);
	return val.w;
}

void rf12_init(void)
{	LED_POWER=1;
#ifdef USE_RSSI
	ADMUX=(1<<REFS0)|(1<<REFS1)|RSSIADC;// ADC für RSSI
#endif
	RF_PORT=(1<<CS);
	RF_DDR&=~((1<<SDO));
	RF_DDR|=(1<<SDI)|(1<<SCK)|(1<<CS);
	SPCR=(1<<SPE)|(1<<MSTR);

	TCCR1A=0;							// Timer für Timeouts
	TCCR1B=(1<<WGM12)|1;
	OCR1A=((F_CPU+2500)/500)-1;
	TIMSK=(1<<OCIE1A);

	for (unsigned char i=0; i<20; i++)
		_delay_ms(10);					// wait until POR done
	rf12_trans(0xC0E0);					// AVR CLK: 10MHz
	rf12_trans(0x80D7);					// Enable FIFO
	rf12_trans(0xC2AB);					// Data Filter: internal
	rf12_trans(0xCA81);					// Set FIFO mode
	rf12_trans(0xE000);					// disable wakeuptimer
	rf12_trans(0xC800);					// disable low duty cycle
	rf12_trans(0xC4B7);					// AFC settings: autotuning: -10kHz...+7,5kHz
}

void rf12_config(unsigned short baudrate, unsigned short freq, unsigned char power, unsigned char environment)
{
	rf12_setfreq(freq); 					// Sende/Empfangsfrequenz einstellen
   	rf12_setpower(0, 5);					// 6mW Ausgangangsleistung, 90kHz Frequenzshift
   	rf12_setbandwidth(4, environment, 1);	// 200kHz Bandbreite, Verstärkung je nach Umgebungsbedingungen, DRSSI threshold: -97dBm (-environment*6dB)
	rf12_setbaud(baudrate);					// Baudrate
}

void rf12_rxmode(void)
{
	rf12_trans(0x82C8);					// RX on
	rf12_trans(0xCA81);					// set FIFO mode
	_delay_ms(.8);
	rf12_trans(0xCA83);					// enable FIFO: sync word search
}

void rf12_stoprx(void)
{
	rf12_trans(0x8208);					// RX off
	_delay_ms(1);
}

void rf12_setbandwidth(unsigned char bandwidth, unsigned char gain, unsigned char drssi)
{
	rf12_trans(0x9500|((bandwidth&7)<<5)|((gain&3)<<3)|(drssi&7));
}

void rf12_setfreq(unsigned short freq)
{	if (freq<96)						// 430,2400MHz
		freq=96;
	else if (freq>3903)					// 439,7575MHz
		freq=3903;
	rf12_trans(0xA000|freq);
}

void rf12_setbaud(unsigned short baud)
{
	if (baud<664)
		baud=664;
	if (baud<5400)						// Baudrate= 344827,58621/(R+1)/(1+CS*7)
		rf12_trans(0xC680|((43104/baud)-1));	// R=(344828/8)/Baud-1
	else
		rf12_trans(0xC600|((344828UL/baud)-1));	// R=344828/Baud-1
}

void rf12_setpower(unsigned char power, unsigned char mod)
{	
	rf12_trans(0x9800|(power&7)|((mod&15)<<4));
}

static inline void rf12_ready(void)
{
	cbi(RF_PORT, CS);
	asm("nop");
	asm("nop");
	while (!(RF_PIN&(1<<SDO)));			// wait until FIFO ready
	sbi(RF_PORT, CS);
}

unsigned rf12_data(void)
{	unsigned char status;
	cbi(RF_PORT, CS);
	asm("nop");
	asm("nop");
	status=RF_PIN&(1<<SDO);
	sbi(RF_PORT, CS);
	if(status)
		return 1;
	else
		return 0;
}

void rf12_txbyte(unsigned char val)
{
	rf12_ready();
	rf12_trans(0xB800|val);
	if ((val==0x00)||(val==0xFF))		// Stuffbyte einfügen um ausreichend Pegelwechsel zu haben
	{	rf12_ready();
		rf12_trans(0xB8AA);
	}
}

unsigned char rf12_rxbyte(void)
{	unsigned char val;
	rf12_ready();
	val =rf12_trans(0xB000);
	if ((val==0x00)||(val==0xFF))		// Stuffbyte wieder entfernen
	{	rf12_ready();
		rf12_trans(0xB000);
	}
	return val;
}

void rf12_txdata(unsigned char *data, unsigned char number, unsigned char status, unsigned char id)
{	unsigned char i;
	unsigned short crc;
	LED_TX=1;
	rf12_trans(0x8238);					// TX on
	rf12_ready();
	rf12_trans(0xB8AA);					// Sync Data
	rf12_ready();
	rf12_trans(0xB8AA);
	rf12_ready();
	rf12_trans(0xB8AA);
	rf12_ready();
	rf12_trans(0xB82D);
	rf12_ready();
	rf12_trans(0xB8D4);
	crc=_crc_ccitt_update (0, status);
	rf12_txbyte(status);				// Status
	crc=_crc_ccitt_update (crc, number);
	rf12_txbyte(number);				// Anzahl der zu sendenden Bytes übertragen
	if (number)							// nur Status ? Dann keine Daten senden
	{	crc=_crc_ccitt_update (crc, id);
		rf12_txbyte(id);				// Paket ID
		for (i=0; i<number; i++)
		{	rf12_txbyte(*data);
			crc=_crc_ccitt_update (crc, *data);
			data++;
		}
	}
	rf12_txbyte(crc);					// Checksumme hinterher senden
	rf12_txbyte(crc/256);				// Checksumme hinterher senden
	rf12_txbyte(0);						// dummy data
	rf12_txbyte(0);						// dummy data
	rf12_trans(0x8208);					// TX off
	LED_TX=0;
}

unsigned char rf12_rxdata(unsigned char *data, unsigned char *status, unsigned char *id)
{	unsigned char i, number;
	unsigned short crc, crcref;
	LED_RX=1;
	*status=rf12_rxbyte();					// Status
	crc=_crc_ccitt_update (0, *status);
#ifdef USE_RSSI
	ADCSRA=(1<<ADEN)|(1<<ADSC)|(1<<ADIE)|7;	// RX Pegel messen
#endif
	number=rf12_rxbyte();					// Anzahl der zu empfangenden Bytes
	crc=_crc_ccitt_update (crc, number);
	if (number>MAX_BUF)
		number=MAX_BUF;
	if (number)
	{	*id=rf12_rxbyte();					// Paketnummer
		crc=_crc_ccitt_update (crc, *id);
		for (i=0; i<number; i++)
		{
#ifdef USE_RSSI
			ADCSRA=(1<<ADEN)|(1<<ADSC)|(1<<ADIE)|7;	// RX Pegel messen
#endif
			*data=rf12_rxbyte();
			crc=_crc_ccitt_update (crc, *data);
			data++;
		}
	}
#ifdef USE_RSSI
	ADCSRA=(1<<ADEN)|(1<<ADSC)|(1<<ADIE)|7;	// RX Pegel messen
#endif
	crcref=rf12_rxbyte();					// CRC empfangen
	crcref|=rf12_rxbyte()*256;				// CRC empfangen
	rf12_trans(0xCA81);						// restart syncword detection:
	rf12_trans(0xCA83);						// enable FIFO
	LED_RX=0;
	if (crcref!=crc)
		return 255;							// checksum error
	else
		return number;						// data ok
}

#ifdef USE_RSSI
ISR(SIG_ADC)
{	unsigned short val;
	val=rssi;	
	rssi=val-val/8+ADC;
}
#endif

void check_rx_packet(void)
{	if (rf12_data())
		rx_packet();
}

void rx_packet(void)
{	static unsigned char rx_lastid=255;
	unsigned char rx_id, status;
	rx_cnt=rf12_rxdata(rxbuf, &status, &rx_id);		// komplettes Paket empfangen
	if (rx_cnt<=MAX_BUF)							// Daten gültig (d.h. kein CRC Fehler) ?
	{	if (status&RECEIVED_OK)						// Empfangsbestätigung ?
		{	flags&=~WAITFORACK;						// -> "Warten auf Bestätigung"-Flag löschen
			tx_cnt=0;								// -> Daten als gesendet markieren
			tx_id++;
			LED_RETRANS=0;
			LED_ERR=0;
		}
		if (rx_cnt)									// Daten empfangen
		{	tx_status=RECEIVED_OK;					// zu sendender Status
			tx_packet(0,0,0);						// Empfangsbestätigung senden
			retrans_cnt=RETRY;						// Retry Counter neu starten
			if (rx_id==rx_lastid)					// Handelt es sich um alte Daten?
				rx_cnt=0;
			else
				rx_lastid=rx_id;					// Aktuelle ID speichern
    	}
	}
	else
		rx_cnt=0;
}

unsigned char rx_data_in_buffer(void)
{	return rx_cnt;
}

unsigned char rx_data(unsigned char *data)
{	unsigned char i;
	while (!rx_cnt)
	{	if (rf12_data())
			rx_packet();
	}
	for (i=0; i<rx_cnt; i++)			// Daten weiterleiten
		data[i]=rxbuf[i];
	rx_cnt=0;
	return i;
}

unsigned char tx_data(unsigned char *data, unsigned char size)
{
	tx_status=0;									// zu sendender Status
	tx_packet(data,size,0);							// erstmaliger Transfer
	while (flags&WAITFORACK)						// warten bis letzter Transfer beendet
	{	if (rf12_data())
			rx_packet();
		if (delaycnt==0)							// Timeout: Daten nochmal senden
		{	LED_RETRANS=1;
			if (retrans_cnt)
			{	retrans_cnt--;
            	tx_packet(data,size, 1);			// retransmit
			}
			else									// Versuche abgelaufen
			{	LED_ERR=1;							// -> Fehler LED an
				tx_cnt=0;							// -> Daten verwerfen
				tx_id++;
				flags&=~WAITFORACK;					// Daten als OK markieren
				return 1;
			}
		}
	}
	return 0;
}

void tx_packet(unsigned char *txbuf, unsigned char size, unsigned char retrans)
{	rf12_stoprx();									// auf TX umschalten
	if ((!retrans)&&((flags&WAITFORACK)||(size==0))) // es wird noch auf eine Antwort vom Paket gewartet oder es sind keine Daten zu senden
	{    rf12_txdata(txbuf, 0, tx_status, 0);		// -> kein komplettes neues Paket, sondern nur Status senden
	}
	else
	{	
		rf12_txdata(txbuf, size, tx_status, tx_id); // komplettes Paket senden
		flags|=WAITFORACK;							// auf ACK warten
		delaycnt=ANSWER_TIMEOUT/2;					// Timeout Counter neu starten
		if (!retrans)								// erstmalige Übertragung ?
			retrans_cnt=RETRY;						// -> Retry Counter neu starten
	}
	rf12_rxmode();									// wieder auf RX umschalten
}

ISR(TIMER1_COMPA_vect)							// 500Hz Interrupt
{	static unsigned short secdiv=1000;
	if (!--secdiv)
	{	secdiv=500;
		secflag=1;
	}
	if (delaycnt)
		delaycnt--;
}
