/*
************************************************************************
*
*   einfache Strommessung mit ESP8266 und INA219
*   ============================================
*                Uwe Berger; 2020
*
*
* Strommessung mit einem INA219-Breakout. Es wird gemessen, wenn 
* START_STOP_PIN auf Low ist. Zusaetzlich werden, bei laufender Messung
* die Impulse am TRIGGER_PIN hochgezaehlt. Mit dem Define
* MEASUREMENT_DELAY kann die Pause zwischen zwei aufeinander folgenden
* Messungen festgelegt werden.
*
* Das Messergebnis wird in folgender Form auf der seriellen Schnitt-
* stelle ausgegeben:
* <Millisekunden seit Start>, <Impulse seit Start>, <Strom in mA>
* 
* Bsp.:
* 4636, 2, 10.000000
*
* Messstart-Zeitpunkt und Impulszaehler werden bei Start der Messung
* (Flanke High --> Low am START_STOP_PIN) zurueckgesetzt.
*
* Mit
*   MEASUREMENT_DELAY:      0
*   CPU Frequenz:           160Mhz (ESP8266)
*   serielle Schnittstelle: 230400 baud
* sollte man eine (zeitliche) Aufloesung von ca. 1ms schaffen. D.h.
* auch, dass man eine Messintervalllaenge von ca. 10ms haben moechte,
* sollte MEASUREMENT_DELAY 9 betragen, etc..
*
*
* ---------
* Have fun!
*
************************************************************************
*/


#include <Wire.h>
#include <Adafruit_INA219.h>

#define TRIGGER_PIN       13   // GPIO13/D7
#define START_STOP_PIN    12   // GPIO12/D6

#define MEASUREMENT_DELAY 0    // ms

#define MEASUREMENT_STOP_SECTION  12


Adafruit_INA219 ina219;

volatile uint8_t section = 0;
volatile uint32_t millis_begin;
volatile bool measurement = false;

// **************************************************************
void print_ina219_current_csv(uint32_t ts, uint8_t section)
{
  float current_mA = ina219.getCurrent_mA();
  Serial.printf("%li, %i, %f\n", ts, section, current_mA);
}

// **************************************************************
ICACHE_RAM_ATTR void trigger_handler(void)
{
  //if (digitalRead(START_STOP_PIN)) section++;

  // Start Messung
  if (section == 0) {
    millis_begin = millis();
	measurement = true;  
  }

  section++;
    
  // Stopp Messung
  if (section == 12) {
	measurement = false;
	section = 0;  
  }


}

/*
// **************************************************************
ICACHE_RAM_ATTR void start_handler(void)
{
  // Start Messung --> ein paar Werte initialiseren
  section = 0;
  millis_begin = millis();
}
*/

// **************************************************************
void setup() {

  // serielle Schnittstelle initialisieren
  Serial.begin(230400);
  Serial.println("");
  // Trigger-Pins konfigurieren
  pinMode(TRIGGER_PIN, INPUT_PULLUP); 
  pinMode(START_STOP_PIN, INPUT_PULLUP); 
  attachInterrupt(digitalPinToInterrupt(TRIGGER_PIN), trigger_handler, FALLING);
  //attachInterrupt(digitalPinToInterrupt(START_STOP_PIN), start_handler, RISING);
  // INA219: initialieren
  if (! ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
    while (1) { delay(10); }
  }
  // INA219: Bereich setzen
  //ina219.setCalibration_32V_1A();
  ina219.setCalibration_16V_400mA();
  Serial.println("Ammeter ready!");
}

// **************************************************************
// **************************************************************
// **************************************************************
void loop() {
  
  //if (digitalRead(START_STOP_PIN)) print_ina219_current_csv(millis()-millis_begin, section);
  if (measurement || !digitalRead(START_STOP_PIN)) print_ina219_current_csv(millis()-millis_begin, section);
  delay(MEASUREMENT_DELAY);
}
