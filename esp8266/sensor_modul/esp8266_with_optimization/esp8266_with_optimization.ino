/*
************************************************************************
*
*   Wetterstation auf Basis ESP8266 (Version 1)
*   ===========================================
*               Uwe Berger; 2020
*
* Sensoren:
* ---------
* * BME280 (Temperatur, Luftdruck, Luftfeuchtigkeit)
* * BH1750 (Helligkeit)
* * SHT15  (Tempertatur; Luftfeuchtigkeit)
* * TMP36  (Temperatur) via einem ADS1115
*
* Messergebnisse werden via MQTT versendet. Zwischen den Messungen
* wird der MC und die Sensoren in einen stromsparenden Zustand versetzt.
*
* Optimierungen (im Vergleich zu Version 1):
* ------------------------------------------
* * Reduzierung Stromverbrauch:
*   ** Wifi-Modul erst aktivieren, wenn wirklich etwas gesendet werden
*      soll 
*   ** Sensoren gleich nach Messung ausschalten
* 
* * Reduzierung Wachzeit:
*   ** Laden/Schreiben der Wifi-Konfiguration in dem Flash unterbinden
*   ** feste IP-Konfiguration verwenden (kein DHCP)
*   ** Daten des letzten verwendeten Wifi-APs zuerst versuchen (kein
*      Scannen nach APs)
*
* ---------
* Have fun!
*
************************************************************************
*/

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_ADS1015.h>
#include <SHT1x.h>
#include <BH1750FVI.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "FS.h"

// WLAN-Konfiguration (Default)
#define WLAN_SSID  "xyz"
#define WLAN_PWD  "42"
#define HOSTNAME  "weatherstation"

// IP-Konfiguration (default)
IPAddress ip(47, 11, 8, 15);
IPAddress gateway(47, 11, 8, 1);
IPAddress subnet(255, 255, 255, 0);

// MQTT-Konfiguration (Default)
#define MQTT_BROKER "brocker"
#define MQTT_PORT   1883
#define MQTT_USER   ""
#define MQTT_PWD    ""
#define MQTT_CLIENT_ID "weatherstation"

char ssid[30]     = WLAN_SSID;
char password[30] = WLAN_PWD;
char hostname[30] = HOSTNAME;

char mqttServer[30] 	= MQTT_BROKER;
int  mqttPort 		    = MQTT_PORT;
char mqttUser[30] 	    = MQTT_USER;
char mqttPassword[30]   = MQTT_PWD;
char mqttClientId[30]   = MQTT_CLIENT_ID;

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

Adafruit_BME280 bme; // I2C

#define MY_ALTITUDE 39.0
float altitude = MY_ALTITUDE;

#define dataPin  D6
#define clockPin D5
SHT1x sht1x(dataPin, clockPin);

BH1750FVI myLux(0x23);

Adafruit_ADS1115 ads;

// Struktur Messwerte
struct {
  float bme_temperature;
  float bme_humidity;
  float bme_pressure_abs;
  float bme_pressure_rel;
  float sht_temperature;
  float sht_humidity;
  float bh1750_luminosity;
  float tmp36_temperature;
  float vcc;
  float vbat;
} sensor_values;

#define DEEPSLEEP_TIME 120e6  // 120s
uint32_t deepsleep_time = DEEPSLEEP_TIME;

uint32_t old_awake_time, awake_time;

// Struktur fuer ESP-RTC-Memory
union rtc_t {
  struct {
	uint32_t crc32;
	byte data[11];  
  } data;
  struct {
    uint32_t crc32;
    uint32_t awake_time;        // 4
    uint8_t wifi_channel;       // 1
    uint8_t wifi_ap_mac[6];     // 6
  } vars;
} rtc;

bool rtc_valid = false;

#define TRIGGER_PIN    13    // GPIO13
#define START_STOP_PIN 15    // GPIO15?

uint8_t debug = 0;


// **************************************************************
float tmp36_readTemperatureC(void)
{
  int adc_value = analogRead(A0);
  float voltage = adc_value/1024.0;
  return (voltage - 0.5) * 100;
}

// **************************************************************
float tmp36_readTemperatureC_via_ads1115 (void)
{
  // 4x Gain (+/- 1.024V 15Bit-Aufloesung)
  ads.setGain(GAIN_FOUR);
  // TMP36 an ADC0 angeschlossen
  int16_t adc = ads.readADC_SingleEnded(0);
  //Serial.printf("--> ADC0 = %i\n", adc);
  float voltage = adc*0.00003125;             // 1.024/2^15
  return (voltage - 0.5) * 100;               // RTFM TMP36
}

// **************************************************************
float read_vcc_via_ads1115 (void)
{
  // 1x Gain (+/- 4.096V --> Aufloesung 2mV)
  ads.setGain(GAIN_ONE);
  // Vcc liegt an ADC1 an
  int16_t adc = ads.readADC_SingleEnded(1);
  //Serial.printf("--> ADC1 = %i\n", adc);
  return adc*0.000125;                       // 4.096/2^15/1000
}

/*
...wir brauchen einen Spannungsteiler, da max. Vcc + 0.3V anliegen
duerfen! Vcc ist 3.3V (MCP1700-3302). Voll geladene Akkus kommen 
locker ueber 3.6V...
// **************************************************************
float read_vbat_via_ads1115 (void)
{
  // 1x Gain (+/- 4.096V --> Aufloesung 2mV)
  ads.setGain(GAIN_ONE);
  // Vcc liegt an ADC2 an
  int16_t adc = ads.readADC_SingleEnded(2);
  //Serial.printf("--> ADC2 = %i\n", adc);
  return adc*0.000125;                       // 4.096/2^15/1000
}
*/

// **************************************************************
float read_vbat_via_ads1115 (void)
{
  // 2x Gain (+/- 2.048V --> 15Bit-Aufloesung)
  ads.setGain(GAIN_TWO);
  // Vcc liegt an ADC2 an
  int16_t adc = ads.readADC_SingleEnded(2);
  //Serial.printf("--> ADC2 = %i\n", adc);
  return adc*3.2*0.0000625;                       // 2048/2^15/1000 * 3.2 (3.2 --> Spannungsteiler)
}



// **************************************************************
// https://github.com/esp8266/Arduino/blob/master/libraries/esp8266/examples/RTCUserMemory/RTCUserMemory.ino
//
uint32_t calculateCRC32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xffffffff;
  while (length--) {
    uint8_t c = *data++;
    for (uint32_t i = 0x80; i > 0; i >>= 1) {
      bool bit = crc & 0x80000000;
      if (c & i) {
        bit = !bit;
      }
      crc <<= 1;
      if (bit) {
        crc ^= 0x04c11db7;
      }
    }
  }
  return crc;
}

// **************************************************************
void write_rtc_memory(uint32_t time)
{
  rtc.vars.awake_time = time;
  rtc.vars.wifi_channel = WiFi.channel();
  memcpy(rtc.vars.wifi_ap_mac, WiFi.BSSID(), 6);
  rtc.vars.crc32 = calculateCRC32((uint8_t*) &rtc.data.data[0], sizeof(rtc.data.data));
  ESP.rtcUserMemoryWrite(0, (uint32_t *) &rtc.data, sizeof(rtc.data));
}

// **************************************************************
void wifi_connect()
{
  uint8_t count = 0;
  
  // Wifi anschalten
  WiFi.forceSleepWake();
  delay(1);
  // kein Laden/Sichern der Wifi-Konfiguration im Flash
  WiFi.persistent( false );
  // ins WIFI anmelden
  WiFi.mode(WIFI_STA);
  // feste IP-Konfiguration
  WiFi.config(ip, gateway, subnet); 
  // wenn Daten im RTC valid, dann mit diesen zuerst den Connect versuchen
  if (rtc_valid) {
	if (debug) Serial.println("Wifi.begin() mit RTC-Daten.");
	WiFi.begin(ssid, password, rtc.vars.wifi_channel, rtc.vars.wifi_ap_mac, true);  
  } else {
	if (debug) Serial.println("Wifi.begin() ohne RTC-Daten.");
    WiFi.begin(ssid, password);
  }
  WiFi.hostname(hostname);
  if (debug) Serial.print("Connect to WiFi: ") ;
  while (WiFi.status() != WL_CONNECTED) {
	count++;
    // nach 5s keine Verbindung mit RTC-Daten zustande gekommen, also doch scannen	
	if (count == 100) {
      if (debug) Serial.println("neuer Versuch ohne RTC-Daten!!!");
	  WiFi.disconnect();
      delay(10);
      WiFi.forceSleepBegin();
      delay(10);
      WiFi.forceSleepWake();
      delay(10);
      WiFi.begin(ssid, password);	
    }
	// nach 15s keine Verbindug zustande gekommen
	if (count == 300) {
	  if (debug) {
	    Serial.println("...no connection (WLAN)!!!");
	    Serial.flush();
	  }
	  write_rtc_memory((rtc.vars.awake_time+millis()-awake_time));
	  WiFi.disconnect(true);
      delay(1);
      WiFi.mode( WIFI_OFF );
	  ESP.deepSleep(deepsleep_time);
	}
    delay(50);
    if (debug) Serial.print(".");
  }
  if (debug) {
    Serial.println("");
    Serial.println("WiFi connected!");
    Serial.println(WiFi.localIP());
    Serial.println("");
  }
}

// **************************************************************
void sensors_read(void)
{
  sensor_values.bme_temperature   = bme.readTemperature();
  sensor_values.bme_humidity      = bme.readHumidity();
  sensor_values.bme_pressure_abs  = bme.readPressure()/100.0F;
  sensor_values.bme_pressure_rel  = sensor_values.bme_pressure_abs+(altitude/8.0);
  sensor_values.sht_temperature   = sht1x.readTemperatureC();
  sensor_values.sht_humidity      = sht1x.readHumidity();
  sensor_values.bh1750_luminosity = myLux.getLux();
  sensor_values.tmp36_temperature = tmp36_readTemperatureC_via_ads1115();
  sensor_values.vcc               = read_vcc_via_ads1115();
  sensor_values.vbat              = read_vbat_via_ads1115();
  // --> delay?
  // Sensoren wieder runterfahren
  myLux.powerOff();               // BH1750
  bme280_sleep();                 // BME280
  // --> SHT15 geht von allein in Sleep-Mode (Datenblatt), was ist mit Heater (S.8)?
  // --> internen ADC ausschalten?
  // --> delay?
}

// **************************************************************
void mqtt_publish_values(void)
{
  char json[500];
  char bme280[100];
  char sht15[100];
  char tmp36[100];
  char bh1750[100];
  char esp[100];
  
  // JSON-String zusammenbauen
  sprintf(bme280, "\"BME280\":{\"temperature\":%.1f,\"humidity\":%.1f,\"pressure_abs\":%.1f,\"pressure_rel\":%.1f}", 
         sensor_values.bme_temperature, sensor_values.bme_humidity, sensor_values.bme_pressure_abs, sensor_values.bme_pressure_rel);
  sprintf(sht15, "\"SHT15\":{\"temperature\":%.1f,\"humidity\":%.1f}", 
         sensor_values.sht_temperature, sensor_values.sht_humidity);
  sprintf(tmp36, "\"TMP36\":{\"temperature\":%.1f}", 
         sensor_values.tmp36_temperature);
  sprintf(bh1750, "\"BH1750\":{\"luminosity\":%.1f}", 
         sensor_values.bh1750_luminosity);
  sprintf(esp, "\"ESP\":{\"awake_time\":%lu,\"vcc\":%.2f,\"vbat\":%.2f}", 
         old_awake_time, sensor_values.vcc, sensor_values.vbat);
  sprintf(json, "{%s, %s, %s, %s, %s}", bme280, sht15, tmp36, bh1750, esp);
  if (debug) Serial.println(json);
  // MQTT-Nachricht versenden
  mqtt_client.publish("new_weatherstation/json/", json, false);
}

// **************************************************************
void mqtt_reconnect ()
{
  uint8_t count = 0;
  // MQTT...
  // ...initialisieren
  mqtt_client.setServer(mqttServer, mqttPort);
  // ...verbinden
  while (!mqtt_client.connected()) {
    if (debug) Serial.println("Connecting to MQTT...");
    if (mqtt_client.connect(mqttClientId, mqttUser, mqttPassword )) {
       if (debug) Serial.println("Connected!");  
    } else {
      if (debug) {
        Serial.print("failed with state ");
        Serial.print(mqtt_client.state());
      }
      count++;
      delay(100);
    }
	if (count == 100) {
	  // keine Verbindung zum Broker, dann wieder schlafen legen
      if (debug) {
        Serial.println("...no connection (MQTT-Broker)!!!");
	    Serial.flush();
      }
      write_rtc_memory((rtc.vars.awake_time+millis()-awake_time));
	  ESP.deepSleep(deepsleep_time);
	}
  }
}

// **************************************************************
String config_getvalue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

// **************************************************************
void config_string2chararray(String val, char * c, uint8_t len)
{
  val.toCharArray(c, len-1);
  // zur Sicherheit nullterminieren
  c[len-1] = '\0';
}


// **************************************************************
void config_read() 
{
  String key, val;
  SPIFFS.begin(); 
  File f = SPIFFS.open( "/config.txt", "r");
  if (!f) {
    if (debug) Serial.println("config file open failed");
  }
  while(f.available()) {
      String line = f.readStringUntil('\n');
      key = config_getvalue(line, '=', 0);
      val = config_getvalue(line, '=', 1);
      if (key.equalsIgnoreCase("wlan_ssid")) {
		config_string2chararray(val, ssid, sizeof ssid);
	  } else 
	  if (key.equalsIgnoreCase("wlan_pwd")) {
		config_string2chararray(val, password, sizeof password);
	  } else 
	  if (key.equalsIgnoreCase("ipadress")) {
        ip.fromString(val);
	  } else 	  
	  if (key.equalsIgnoreCase("gateway")) {
        gateway.fromString(val);
	  } else 	  
	  if (key.equalsIgnoreCase("subnet")) {
        subnet.fromString(val);
	  } else 
	  if (key.equalsIgnoreCase("hostname")) {
		config_string2chararray(val, hostname, sizeof hostname);
	  } else 
	  if (key.equalsIgnoreCase("mqtt_brocker")) {
		config_string2chararray(val, mqttServer, sizeof mqttServer);
	  } else 
	  if (key.equalsIgnoreCase("mqtt_port")) {
		mqttPort=val.toInt();  
	  } else 
	  if (key.equalsIgnoreCase("mqtt_user")) {
		config_string2chararray(val, mqttUser, sizeof mqttUser);
	  } else 
	  if (key.equalsIgnoreCase("mqtt_pwd")) {
		config_string2chararray(val, mqttPassword, sizeof mqttPassword);
	  } else 
	  if (key.equalsIgnoreCase("mqtt_client_id")) {
		config_string2chararray(val, mqttClientId, sizeof mqttClientId);
	  }	else 
	  if (key.equalsIgnoreCase("debug")) {
		debug=val.toInt();  
	  }	else 
	  if (key.equalsIgnoreCase("deepsleep_time")) {
		deepsleep_time=val.toInt();  
	  }	else 
	  if (key.equalsIgnoreCase("altitude")) {
		altitude=val.toFloat();  
	  } 	  
  }
  f.close();
}

// **************************************************************
void bme280_sleep(void)
{
  // BME280 in Sleep-Mode versetzen --> RTFM	
  Wire.beginTransmission(0x76);   // i2c-Adr. BME280         
  Wire.write(0xF4);               // BME280_REGISTER_CONTROL  
  Wire.write(0b00);               // MODE_SLEEP              
  Wire.endTransmission();         
}

// **************************************************************
void pulse_pin(uint8_t pin)
{
  digitalWrite(pin, LOW);
  delay(1);
  digitalWrite(pin, HIGH);	
}

// **************************************************************
void setup() 
{

  awake_time = millis();
  
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(START_STOP_PIN, OUTPUT);
  digitalWrite(TRIGGER_PIN, HIGH);
  digitalWrite(START_STOP_PIN, LOW);
  
  digitalWrite(START_STOP_PIN, HIGH);
  pulse_pin(TRIGGER_PIN);      // ==> 1

  // Wifi ausschalten, da zuerst nur die Sensoren etc. ausgelesen werden sollen
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  //WiFi.setSleepMode(WIFI_MODEM_SLEEP);
  delay(1);

  pulse_pin(TRIGGER_PIN);  // ==> 2

  // RTC-Memory auslesen und validieren
  ESP.rtcUserMemoryRead(0, (uint32_t *) &rtc.data, sizeof(rtc.data));
  if (calculateCRC32((uint8_t*) &rtc.data.data[0], sizeof(rtc.data.data)) == rtc.vars.crc32) {
    old_awake_time = rtc.vars.awake_time;
    rtc_valid = true;
  } else {
    old_awake_time = 0;
  }

  pulse_pin(TRIGGER_PIN);  // ==> 3

  // Konfiguration aus Datei im SPIFFS auslesen
  config_read();

  pulse_pin(TRIGGER_PIN);  // ==> 4

  // serielle Schnittstelle initialisieren
  if (debug) {
    Serial.begin(115200);
    Serial.println("");
    Serial.println("...setup");
  }
  
  Wire.setClock(3400000);
  
  // BME280
  bool status = bme.begin(0x76);  
  if (!status) {
    if (debug) Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
  
  // BH1750
  myLux.powerOn();
  myLux.setContHighRes();
  
  // ADS1115
  ads.begin();

  pulse_pin(TRIGGER_PIN);  // ==> 5

  // Sensoren auslesen und in Stromsparmode schicken (wenn erforderlich)
  sensors_read();
  if (debug) {
    Serial.println("sensor_read ended.");
    Serial.flush();
  }

  pulse_pin(TRIGGER_PIN);  // ==> 6

  // WiFi initialisieren (incl. Aktivierung Wifi)
  wifi_connect();

  pulse_pin(TRIGGER_PIN);  // ==> 7

  // MQTT initialisieren
  mqtt_reconnect();
  
  pulse_pin(TRIGGER_PIN);  // ==> 8

  // ADC nochmal auslesen, irgendwie stimmt am Anfang das Ergebnis nicht...
  //sensor_values.tmp36_temperature = tmp36_readTemperatureC();
  
  // MQTT
  mqtt_publish_values();
  // MQTT-Loop aufrufen, um Telegramm sicher zu versenden
  //mqtt_client.loop();
  delay(1);

  pulse_pin(TRIGGER_PIN);  // ==> 9

  // ESP schlafen legen
  if (debug) {
    Serial.println("...deepSleep");
    Serial.flush();
  }
  
  // aktuelle Wachzeit sowie CRC32 berechnen und in RTC-Memory schreiben
  write_rtc_memory((millis()-awake_time));
  
  pulse_pin(TRIGGER_PIN);  // ==> 10

  // schlafen gehen
  //WiFi.disconnect(true);
  //delay(1);
  
  pulse_pin(TRIGGER_PIN);  // ==> 11
  digitalWrite(START_STOP_PIN, LOW);

  ESP.deepSleep(deepsleep_time, WAKE_RF_DISABLED);
  
}

// **************************************************************
// **************************************************************
// **************************************************************
void loop() 
{ 

}

