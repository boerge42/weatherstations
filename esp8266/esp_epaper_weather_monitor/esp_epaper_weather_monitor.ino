/*
************************************************************************

Weather-Monitor (MQTT --> E-Paper)
      (ESP8266-Version)
==================================
       Uwe Berger; 2021

Hardware:
---------
* ESP8266-Modul
* E-Paper (Waveshare 1.54inch Module (B))
  ** https://www.waveshare.com/wiki/1.54inch_e-Paper_Module_(B)
  ** 200x200 Pixel

mapping e-Paper to Wemos D1 mini
--> BUSY -> D2, RST -> D1, DC -> D3, CS -> D8, CLK -> D5, DIN -> D7, GND -> GND, 3.3V -> 3.3V

mapping e-Paper to generic ESP8266
--> BUSY -> GPIO4, RST -> GPIO5, DC -> GPIO0, CS -> GPIO15, CLK -> GPIO14, DIN -> GPIO13, GND -> GND, 3.3V -> 3.3V


required libraries (Arduino):
-----------------------------
* ESP8266WiFi
* PubSubClient
* ezTime
* ArduinoJson
* GxEPD
* Adafruit_GFX

MQTT-Topic:
-----------
new_weatherstation/json/ 

MQTT-Payload:
-------------
{
	"BME280":{"temperature":18.2,"humidity":34.6,"pressure_abs":1017.3,"pressure_rel":1022.2}, 
	"SHT15":{"temperature":17.3,"humidity":42.6}, 
	"TMP36":{"temperature":20.7}, 
	"BH1750":{"luminosity":76.7}, 
	"ESP":{"awake_time":1919,"vcc":3.67,"vbat":4.01}
}

...run <prog-name> -?

---------
Have fun!

************************************************************************
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ezTime.h>
#include <ArduinoJson.h>
#include <Ticker.h>

// e-Paper-Zeugs
#include <GxEPD.h>
#include <GxGDEW0154Z04/GxGDEW0154Z04.h>  // 1.54" b/w/r 200x200
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

// FreeFonts from Adafruit_GFX
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

const GFXfont* f1 = &FreeMonoBold9pt7b;
const GFXfont* f2 = &FreeMonoBold12pt7b;
const GFXfont* f3 = &FreeMono9pt7b;

GxIO_Class io(SPI, /*CS=D8*/ SS, /*DC=D3*/ 0, /*RST=D1*/ 5); // RST --> D1 --> GPIO05
GxEPD_Class display(io, /*RST=D1*/ 5, /*BUSY=D2*/ 4); // RST --> D1 --> GPIO05

// WLAN-Konfiguration
#define WLAN_SSID	"xxxx"
#define WLAN_PWD	"yyyy"
#define HOSTNAME	"epaper_esp"

// MQTT-Konfiguration
#define MQTT_BROKER		"10.1.1.82"
#define MQTT_PORT		1883                           
#define MQTT_USER		""
#define MQTT_PWD    	""
#define MQTT_CLIENT_ID	"epaper_esp"

#define TOPIC_NEW_WEATHERSTATION_JSON "new_weatherstation/json/"

#define MY_TIME_ZONE "Europe/Berlin"

char ssid[30] 	  = WLAN_SSID;
char password[30] = WLAN_PWD;
char hostname[30] = HOSTNAME;

char mqttServer[30] 	= MQTT_BROKER;
int  mqttPort 		    = MQTT_PORT;
char mqttUser[30] 	    = MQTT_USER;
char mqttPassword[30]   = MQTT_PWD;
char mqttClientId[30]   = MQTT_CLIENT_ID;

// anzuzeigende Daten
typedef struct {
  float temperature;
  float humidity;
  float pressure_rel;
  float luminosity;
  float awake_time;
  float vcc;
  //char  last_update[20];
  String  last_update;
} weather_values_t;
weather_values_t v = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, "no update!"};

// Definition der anzuzeigenden Daten aus JSON-Nachricht
typedef struct {
	char  key[20];
	char  value[20];
} json_key_t;
json_key_t temperature = {"TMP36",  "temperature"};
json_key_t humidity    = {"SHT15",  "humidity"};
json_key_t pressure    = {"BME280", "pressure_rel"};
json_key_t luminosity  = {"BH1750", "luminosity"};
json_key_t vcc         = {"ESP",    "vbat"};
json_key_t awake_time  = {"ESP",    "awake_time"};

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

Timezone MyZone;
char my_time_zone[50] = MY_TIME_ZONE;

Ticker refresh_timer;

uint8_t refresh = 1;


// *********************************************************************
void draw_frame(uint16_t x0, uint16_t y0, uint8_t w, uint8_t h, const char * str, const GFXfont* f)
{
	int16_t  x1, y1;
	uint16_t w1, h1;
	
	display.setFont(f);
	display.getTextBounds(str, x0, y0, &x1, &y1, &w1, &h1);
	display.drawRect(x0, y0, w, h, GxEPD_BLACK);
	display.fillRect(x1, y1+h1, w1+6, h1+6, GxEPD_BLACK);
	display.setTextColor(GxEPD_WHITE);
	display.setCursor(x0+3, y0+h1+3);
	display.print(str);
}

// *********************************************************************
void showValues()
{
  uint8_t y = 40;
  uint8_t x = 65;
  uint8_t xa = 4;
  char buf[50];

  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);

  display.setFont(f2);
  display.setCursor(x, y);
  sprintf(buf, "%6.1fC", v.temperature);
  display.print(buf);
  display.setFont(f1);
  display.setCursor(xa, y);
  sprintf(buf, "%.3s", temperature.key);
  display.print(buf);
  y=y+24;

  display.setFont(f2);
  display.setCursor(x, y);
  sprintf(buf, "%6.1f%%H", v.humidity);
  display.print(buf);
  display.setFont(f1);
  display.setCursor(xa, y);
  sprintf(buf, "%.3s", humidity.key);
  display.print(buf);
  y=y+24;
  
  display.setFont(f2);
  display.setCursor(x, y);
  sprintf(buf, "%6.1fhPa", v.pressure_rel);
  display.print(buf);
  display.setFont(f1);
  display.setCursor(xa, y);
  sprintf(buf, "%.3s", pressure.key);
  display.print(buf);
  y=y+24;
  
  display.setFont(f2);
  display.setCursor(x, y);
  sprintf(buf, "%6.1flux", v.luminosity);
  display.print(buf);
  display.setFont(f1);
  display.setCursor(xa, y);
  sprintf(buf, "%.3s", luminosity.key);
  display.print(buf);

  y=154;
  display.setFont(f2);
  display.setCursor(20, y);
  sprintf(buf, "%8.3fs", v.awake_time);
  display.print(buf);
  y=y+24;
  display.setFont(f2);
  display.setCursor(20, y);
  sprintf(buf, "%7.2f V", v.vcc);
  display.print(buf);
 
  draw_frame(2, 2, 197, 116, "WEATHER", f1);
  draw_frame(2, 120, 197, 64, "ESP", f1);
  
  display.setFont(f3);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(2, 197);
  display.print(v.last_update);
}

// *********************************************************************
void refresh_display(void)
{
  Serial.println("--> refresh_display");
  refresh = 1;
}

// *********************************************************************
void wifi_connect()
{
  // ins WIFI anmelden 
  WiFi.begin(ssid, password);
  //WiFi.setHostname(hostname);
  Serial.print("Connect to WiFi: ") ;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Connected!");
  Serial.println(WiFi.localIP());
}

// *********************************************************************
void mqtt_callback(char* topic, byte* payload, unsigned int length) 
{
  Serial.println("--> mqtt_callback");
  // JSON-Payload auseinander nehmen
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, payload);
  // ist es ueberhaupt json...
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  // definierte Daten entspr. merken
  v.temperature  = doc[temperature.key][temperature.value];
  v.humidity     = doc[humidity.key][humidity.value];
  v.pressure_rel = doc[pressure.key][pressure.value];
  v.luminosity   = doc[luminosity.key][luminosity.value];
  v.awake_time   = doc[awake_time.key][awake_time.value];
  v.awake_time   = v.awake_time/1000;
  v.vcc          = doc[vcc.key][vcc.value];
  v.last_update = String(MyZone.dateTime("d.m.y; H:i:s"));
  //display.drawPaged(showValues);
}

// *********************************************************************
void mqtt_reconnect ()
{
  // MQTT...
  // ...initialisieren
  mqtt_client.setServer(mqttServer, mqttPort);
  // ...Callback setzen
  mqtt_client.setCallback(mqtt_callback);
  // ...verbinden
  while (!mqtt_client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (mqtt_client.connect(mqttClientId, mqttUser, mqttPassword )) {
       Serial.println("Connected!");  
    } else {
      Serial.print("failed with state ");
      Serial.print(mqtt_client.state());
      delay(500);
    }
  }
  // ...Topic abonnieren
  mqtt_client.subscribe(TOPIC_NEW_WEATHERSTATION_JSON);
}

// *********************************************************************
void setup(void)
{
  // UART
  Serial.begin(115200);
  Serial.println();
  Serial.println("setup");

  // WiFi
  wifi_connect();
  
  // NTP-Time
  waitForSync();
  Serial.println("UTC: " + UTC.dateTime());
  MyZone.setLocation(my_time_zone);
  //Serial.println("MyZone time: " + MyZone.dateTime("d-M-y H:i"));
  Serial.println("MyZone time: " + MyZone.dateTime());
  
  // Display
  display.init(115200); // enable diagnostic output on Serial
  display.setRotation(2);
  
  // Timer
  refresh_timer.attach(600.0, refresh_display); // 10min

  Serial.println("setup done");
}

// *********************************************************************
// *********************************************************************
// *********************************************************************
void loop()
{
  // MQTT-Loop
  if (!mqtt_client.connected()) {
    mqtt_reconnect();
  }
  mqtt_client.loop();
  
  // Display
  if (refresh == 1) {
	refresh = 0;
	display.drawPaged(showValues); 
  }
}
