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
* Diese Programmversion ist der erste Wurf! Im Vordergrund stand die 
* Funktionalität (Sensoren abfragen, Messwerte via MQTT versenden, MCU 
* in Schlafmode schicken sowie wieder aufwachen lassen).
* Detailierte Verbesserungen im Hinblick auf Reduzierung des Stromver-
* brauchs und der Wachzeit werden in einer nächsten Version eingebaut.
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

#include <ezTime.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "FS.h"

// WLAN-Konfiguration (Default)
#define WLAN_SSID  "xyz"
#define WLAN_PWD  "42"
#define HOSTNAME  "my_pxmatrix"

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

#define DEEPSLEEP_TIME 120e6  // 120s
uint32_t deepsleep_time = DEEPSLEEP_TIME;

uint32_t old_awake_time, awake_time;

// Struktur fuer ESP-RTC-Memory
union rtc_t {
  struct {
	uint32_t crc32;
	byte data[4];  
  } data;
  struct {
    uint32_t crc32;
    uint32_t awake_time;
  } vars;
} rtc;

#define TRIGGER_PIN    13    // GPIO13

uint8_t debug = 0;


// **************************************************************
void pulse_pin(uint8_t pin)
{
  digitalWrite(pin, LOW);
  delay(1);
  digitalWrite(pin, HIGH);	
}

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
  return adc*0.000125;                       // 4.096/2^15
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
void wifi_connect()
{
  uint8_t count = 0;
  // ins WIFI anmelden 
  WiFi.begin(ssid, password);
  WiFi.hostname(hostname);
  Serial.print("Connect to WiFi: ") ;
  while (WiFi.status() != WL_CONNECTED) {
	count++;
	if (count > 50) {
	  // keine Verbindung ins WLAN, dann wieder schlafen legen
	  Serial.println("...no connection (WLAN)!!!");
	  Serial.flush();
	  // aktuelle Wachzeit sowie CRC32 berechnen und in RTC-Memory schreiben
      rtc.vars.awake_time = rtc.vars.awake_time + millis() - awake_time;
      rtc.vars.crc32 = calculateCRC32((uint8_t*) &rtc.data.data[0], sizeof(rtc.data.data));
      ESP.rtcUserMemoryWrite(0, (uint32_t *) &rtc.data, sizeof(rtc.data));
	  ESP.deepSleep(deepsleep_time);
	}
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.println(WiFi.localIP());
  Serial.println("");
}

// **************************************************************
void mqtt_publish_values(bool publish)
{
  char json[500];
  char bme280[100];
  char sht15[100];
  char tmp36[100];
  char bh1750[100];
  char esp[100];
  
  String ts_string = UTC.dateTime(RFC3339);
  char ts_char[50];
  
  config_string2chararray(ts_string, ts_char, sizeof ts_char);
  sprintf(bme280, "\"BME280\":{\"temperature\":%.1f,\"humitity\":%.1f,\"pressure_abs\":%.1f,\"pressure_rel\":%.1f}", 
         bme.readTemperature(), bme.readHumidity(), bme.readPressure()/100.0F, bme.readPressure()/100.0F+(altitude/8.0));
  sprintf(sht15, "\"SHT15\":{\"temperature\":%.1f,\"humitity\":%.1f}", 
         sht1x.readTemperatureC(), sht1x.readHumidity());
  sprintf(tmp36, "\"TMP36\":{\"temperature\":%.1f}", 
         tmp36_readTemperatureC_via_ads1115());
  sprintf(bh1750, "\"BH1750\":{\"luminosity\":%.1f}", 
         myLux.getLux());
  sprintf(esp, "\"ESP\":{\"awake_time\":%lu,\"vcc\":%.2f}", 
         old_awake_time, read_vcc_via_ads1115());
  sprintf(json, "{%s, %s, %s, %s, %s}", bme280, sht15, tmp36, bh1750, esp);
  //Serial.println(json);
  //if (publish) mqtt_client.publish("new_weatherstation/json/", json, strlen(json), false);
  if (publish) mqtt_client.publish("new_weatherstation/json/", json, false);
}

// **************************************************************
void mqtt_callback(char* topic, byte* payload, unsigned int length) 
{
	
}

// **************************************************************
void mqtt_reconnect ()
{
  uint8_t count = 0;
  // MQTT...
  // ...initialisieren
  mqtt_client.setServer(mqttServer, mqttPort);
  // ...Callback setzen
  mqtt_client.setCallback(mqtt_callback);
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
      delay(500);
    }
	if (count > 50) {
	  // keine Verbindung zum Broker, dann wieder schlafen legen
	  if (debug) {
	    Serial.println("...no connection (MQTT-Broker)!!!");
	    Serial.flush();
	  }
	  // aktuelle Wachzeit sowie CRC32 berechnen und in RTC-Memory schreiben
      rtc.vars.awake_time = rtc.vars.awake_time + millis() - awake_time;
      rtc.vars.crc32 = calculateCRC32((uint8_t*) &rtc.data.data[0], sizeof(rtc.data.data));
      ESP.rtcUserMemoryWrite(0, (uint32_t *) &rtc.data, sizeof(rtc.data));
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
      if (debug) {
        Serial.print(key);
        Serial.print(" --> ");
        Serial.println(config_getvalue(line, '=', 1));
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
void setup() 
{

  awake_time = millis();
  
  pinMode(TRIGGER_PIN, OUTPUT);
  digitalWrite(TRIGGER_PIN, HIGH);

  pulse_pin(TRIGGER_PIN);      // ==> 1
  
  // RTC-Memory auslesen und validieren
  ESP.rtcUserMemoryRead(0, (uint32_t *) &rtc.data, sizeof(rtc.data));
  if (calculateCRC32((uint8_t*) &rtc.data.data[0], sizeof(rtc.data.data)) == rtc.vars.crc32) {
    old_awake_time = rtc.vars.awake_time;
  } else {
    old_awake_time = 0;
  }

  pulse_pin(TRIGGER_PIN);      // ==> 2

  // serielle Schnittstelle initialisieren
  if (debug) {
    Serial.begin(115200);
    Serial.println("");
    Serial.println("...setup");
  }
  
  // Konfiguration aus Datei im SPIFFS auslesen
  config_read();

  pulse_pin(TRIGGER_PIN);      // ==> 3

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

  pulse_pin(TRIGGER_PIN);      // ==> 4

  // Dummy-Messung
  mqtt_publish_values(false);

  pulse_pin(TRIGGER_PIN);      // ==> 5

  // WiFi initialisieren
  wifi_connect();
  
  pulse_pin(TRIGGER_PIN);      // ==> 6

  // MQTT initialisieren
  mqtt_reconnect();

  pulse_pin(TRIGGER_PIN);      // ==> 7
  
  // Messungen
  mqtt_publish_values(true);
  // MQTT-Loop aufrufen, um Telegramm sicher zu versenden
  mqtt_client.loop();

  pulse_pin(TRIGGER_PIN);      // ==> 8


  // fuer 60s schlafen legen
  if (debug) {
    Serial.println("...deepSleep");
    Serial.flush();
  }
  // Sensoren runterfahren
  myLux.powerOff();               // BH1750
  bme280_sleep();                 // BME280
  // SHT15 geht von allein in Sleep-Mode (Datenblatt), was ist mit Heater (S.8)?
 
  pulse_pin(TRIGGER_PIN);      // ==> 9
  
  // aktuelle Wachzeit sowie CRC32 berechnen und in RTC-Memory schreiben
  rtc.vars.awake_time = millis() - awake_time;
  rtc.vars.crc32 = calculateCRC32((uint8_t*) &rtc.data.data[0], sizeof(rtc.data.data));
  ESP.rtcUserMemoryWrite(0, (uint32_t *) &rtc.data, sizeof(rtc.data));

  pulse_pin(TRIGGER_PIN);      // ==> 10
  pulse_pin(TRIGGER_PIN);      // ==> 11 // damit 11.Impulse wieder Messende ist...
  
  // schlafen gehen
  ESP.deepSleep(deepsleep_time);
  
}

// **************************************************************
// **************************************************************
// **************************************************************
void loop() 
{ 

}

