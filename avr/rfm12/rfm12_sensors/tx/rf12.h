
extern unsigned short rf12_trans(unsigned short wert);					// transfer 1 word to/from module
extern void rf12_init(void);											// initialize module
extern void rf12_setfreq(unsigned short freq);							// set center frequency
extern void rf12_setbaud(unsigned short baud);							// set baudrate
extern void rf12_setpower(unsigned char power, unsigned char mod);		// set transmission settings
extern void rf12_setbandwidth(unsigned char bandwidth, unsigned char gain, unsigned char drssi);	// set receiver settings
extern void rf12_txdata(unsigned char *data, unsigned char number, unsigned char status, unsigned char id);		// transmit number of bytes from array
extern unsigned char rf12_rxdata(unsigned char *data, unsigned char *status, unsigned char *id);	// receive number of bytes into array
extern void rf12_rxmode(void);
extern unsigned rf12_data(void);
extern void rf12_stoprx(void);
extern void rf12_txbyte(unsigned char val);
extern unsigned char rf12_rxbyte(void);
extern void rf12_config(unsigned short baudrate, unsigned short freq, unsigned char power, unsigned char environment);	// config module
extern unsigned char rx_data(unsigned char *data);
extern unsigned char tx_data(unsigned char *data, unsigned char size);
extern void tx_packet(unsigned char *txbuf, unsigned char size, unsigned char retrans);
extern unsigned char rx_data_in_buffer(void);
extern void check_rx_packet(void);

#define RF12FREQ(freq)	((unsigned short)((freq-430.0)/0.0025))			// macro for calculating frequency value out of frequency in MHz

#define QUIET		0
#define NORMAL		1
#define NOISY		2

#define RECEIVED_OK			1		// Daten erfolgreich empfangen
#define RECEIVED_FAIL		2		// Daten fehlerhaft empfangen -> bitte nochmal senden
#define CONTROL_CMD			4		// Steuerbefehl

#define WAITFORACK		1

extern volatile unsigned short rssi;
extern volatile unsigned char delaycnt, secflag;
extern unsigned char flags, tx_cnt, tx_id, tx_status, retrans_cnt;

