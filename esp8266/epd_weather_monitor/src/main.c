/*
************************************************************************

Weather-Monitor (MQTT --> E-Paper)
==================================
       Uwe Berger; 2020

Hardware:
---------
* Raspberry Pi
* E-Paper (Waveshare 1.54inch Module (B))
  ** https://www.waveshare.com/wiki/1.54inch_e-Paper_Module_(B)
  ** 200x200 Pixel
  ** dreifarbig (Weiss/Schwarz/Rot)
  ** Lib --> https://github.com/waveshare/e-Paper
     (unnoetiges Zeugs eleminiert)
  ** Ansteuerung via SPI

required libraries:
-------------------
* wiringpi
* mosquitto
* json-c

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

#include <stdlib.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>

#include <errno.h>
#include <string.h>
#include <mosquitto.h>
#include <json-c/json.h>

#include "DEV_Config.h"
#include "GUI_Paint.h"
#include "Debug.h"
#include "EPD_1in54b.h"


typedef struct {
	char  key[20];
	char  value[20];
} json_key_t;

json_key_t temperature = {"SHT15",  "temperature"};
json_key_t humidity    = {"SHT15",  "humidity"};
json_key_t pressure    = {"BME280", "pressure_rel"};
json_key_t luminosity  = {"BH1750", "luminosity"};
json_key_t vcc         = {"ESP",    "vcc"};
json_key_t awake_time  = {"ESP",    "awake_time"};



typedef struct {
  float temperature;
  float humidity;
  float pressure_rel;
  float luminosity;
  float awake_time;
  float vcc;
  char  last_update[20];
} weather_values_t;

weather_values_t v = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, "no update!"};

// E-Paper
UBYTE *BlackImage, *RedImage;
UWORD Imagesize = ((EPD_1IN54B_WIDTH % 8 == 0)? (EPD_1IN54B_WIDTH / 8 ): (EPD_1IN54B_WIDTH / 8 + 1)) * EPD_1IN54B_HEIGHT;


// MQTT-Defaults
#define MQTT_HOST 				"localhost"
#define MQTT_PORT 				1883
#define MQTT_USER				""
#define MQTT_PWD				""
#define MQTT_CLIENT_ID			"epd_weather_monitor"
#define MQTT_KEEPALIVE 			3600
#define MQTT_TOPIC_MYWEATHER 	"new_weatherstation/json/"

char mqtt_host[50]		= MQTT_HOST;
int  mqtt_port    		= MQTT_PORT;
char mqtt_user[50]		= MQTT_USER;
char mqtt_pwd[50]		= MQTT_PWD;
char mqtt_client_id[50] = MQTT_CLIENT_ID;

struct mosquitto *mosq	= NULL;

#define DISPLAY_REFRESH 600  // in Sekunden...

volatile uint8_t first_display = 1;
volatile uint16_t display_refresh = DISPLAY_REFRESH;


unsigned char debug		= 0; 


// *********************************************************************
void draw_frame(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, const char * str)
{
	Paint_DrawRectangle(x0, y0, x1, y1, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
	Paint_DrawString_EN(x0, y0, str, &Font16, BLACK, WHITE);
}

// *********************************************************************
void display()
{
	char buf[50];
	uint8_t y = 0;

    //EPD_1IN54B_Init();
	// in Schwarz
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);
    y = 22;
    sprintf(buf, "%6.1fC", v.temperature);
    Paint_DrawString_EN(42, y,  buf, &Font24, WHITE, BLACK);
    sprintf(buf, "%.3s", temperature.key);
    Paint_DrawString_EN(4, y+6,  buf, &Font16, WHITE, BLACK);
    y=y+24;
    sprintf(buf, "%6.1f%%H", v.humidity);
    Paint_DrawString_EN(42, y,  buf, &Font24, WHITE, BLACK);
    sprintf(buf, "%.3s", humidity.key);
    Paint_DrawString_EN(4, y+6,  buf, &Font16, WHITE, BLACK);
    y=y+24;
    sprintf(buf, "%6.1fhPa", v.pressure_rel);
    Paint_DrawString_EN(42, y,  buf, &Font24, WHITE, BLACK);
    sprintf(buf, "%.3s", pressure.key);
    Paint_DrawString_EN(4, y+6,  buf, &Font16, WHITE, BLACK);
    y=y+24;
    sprintf(buf, "%6.1flux", v.luminosity);
    Paint_DrawString_EN(42, y,  buf, &Font24, WHITE, BLACK);
    sprintf(buf, "%.3s", luminosity.key);
    Paint_DrawString_EN(4, y+6,  buf, &Font16, WHITE, BLACK);
    y=139;
    sprintf(buf, "%8.3fs", v.awake_time/1000);
    Paint_DrawString_EN(0, y,  buf, &Font24, WHITE, BLACK);
    y=y+24;
    sprintf(buf, "%7.2f V", v.vcc);
    Paint_DrawString_EN(0, y,  buf, &Font24, WHITE, BLACK);
    Paint_DrawString_EN(66, 188, v.last_update, &Font12, WHITE, BLACK);

    draw_frame(2, 2, 199, 116, " WEATHER ");
    draw_frame(2, 120, 199, 185, " ESP ");

	// in Rot
    Paint_SelectImage(RedImage);
    Paint_Clear(WHITE);
//    draw_frame(2, 2, 199, 116, " WEATHER ");
//    draw_frame(2, 120, 199, 185, " ESP ");
    
    // ...und ausgeben
    EPD_1IN54B_Display(BlackImage, RedImage);
    //EPD_1IN54B_Sleep();
    alarm(display_refresh);
}

// *********************************************************************
void alarm_callback(int signal)
{
	if (debug) printf("--> alarm_callback\n");
	display();
	alarm(display_refresh);
}


// *********************************************************************
int mosquitto_error_handling(int error)
{
	switch(error)
    {
        case MOSQ_ERR_SUCCESS:
			return 0;
            break;
        case MOSQ_ERR_INVAL:
        case MOSQ_ERR_NOMEM:
        case MOSQ_ERR_NO_CONN:
        case MOSQ_ERR_PROTOCOL:
        case MOSQ_ERR_PAYLOAD_SIZE:
		case MOSQ_ERR_CONN_LOST:
		case MOSQ_ERR_NOT_SUPPORTED:
		case MOSQ_ERR_ERRNO:
				fprintf(stderr, "Mosquitto-Error(%i): %s\n", error, mosquitto_strerror(errno));
				exit(EXIT_FAILURE);
				break;
    }
	return 0;
}

// ************************************************
void my_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	// im Debug-Mode Mosquitto-Log-Message ausgeben
	if (debug) printf("--> MQTT-LOG: %s\n", str);
}

// ************************************************
void my_disconnect_callback(struct mosquitto *mosq, void *userdata, int i)
{
	if (debug) printf("--> MQTT-DISCONNECT-CALLBACK\n");
	mosquitto_error_handling(mosquitto_reconnect(mosq));

}

// ************************************************
void my_connect_callback(struct mosquitto *mosq, void *userdata, int i)
{
	if (debug) printf("--> MQTT-CONNECT-CALLBACK\n");
	// Topic abonnieren
	mosquitto_error_handling(mosquitto_subscribe(mosq, NULL, MQTT_TOPIC_MYWEATHER, 2));
}

// *********************************************************************
void my_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
	struct json_object *j_root;
    struct tm *tmnow;
	time_t tnow;
	
	if (debug) {
		printf("--> MQTT-MESSAGE:\n");
		printf("Topic: %s\n", (char *)message->topic);
		printf("Payload: %s\n\n", (char *)message->payload);
	}
	// Payload verarbeiten
	if (strcmp((char *)message->topic, MQTT_TOPIC_MYWEATHER) == 0) {
		// Werte aus JSON-String holen
		j_root=json_tokener_parse((char *)message->payload);
		v.temperature = (json_object_get_double(json_object_object_get(json_object_object_get(j_root, temperature.key), temperature.value)));
		v.pressure_rel = (json_object_get_double(json_object_object_get(json_object_object_get(j_root, pressure.key), pressure.value)));
		v.humidity = (json_object_get_double(json_object_object_get(json_object_object_get(j_root, humidity.key), humidity.value)));
		v.luminosity = (json_object_get_double(json_object_object_get(json_object_object_get(j_root, luminosity.key), luminosity.value)));
		v.vcc = (json_object_get_double(json_object_object_get(json_object_object_get(j_root, vcc.key), vcc.value)));
		v.awake_time = (json_object_get_double(json_object_object_get(json_object_object_get(j_root, awake_time.key), awake_time.value)));
		// Timestamp
		time(&tnow);
		tmnow = localtime(&tnow);
		sprintf(v.last_update, "%d/%02d/%02d %02d:%02d:%02d", 
				tmnow->tm_year+1900, tmnow->tm_mon+1, tmnow->tm_mday, tmnow->tm_hour, tmnow->tm_min, tmnow->tm_sec);
		// evtl. auf EPD ausgeben
		if (first_display) {
			first_display = 0;
			display();
		}
    }
}

// *********************************************************************
void my_epd_init(void)
{
    if(DEV_Module_Init()!=0){
        exit(1);
    }
    EPD_1IN54B_Init();
    EPD_1IN54B_Clear();
    if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\r\n");
        exit(1);
    }
    if((RedImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for red memory...\r\n");
        exit(1);
    }
    Paint_NewImage(BlackImage, EPD_1IN54B_WIDTH, EPD_1IN54B_HEIGHT, 180, WHITE);
    Paint_NewImage(RedImage, EPD_1IN54B_WIDTH, EPD_1IN54B_HEIGHT, 180, WHITE);
}

// *********************************************************************
void my_epd_exit(void)
{
	// EPD
    free(BlackImage);
    free(RedImage);
    BlackImage = NULL;
    RedImage = NULL;
	DEV_Module_Exit();
	// Mosquitto
	mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}

// *********************************************************************
void  exit_handler(int signo)
{
    //System Exit
	my_epd_exit();
    exit(0);
}

// *********************************************************************
void check_copy_str_param(char *dest, uint8_t dest_len, char *src, char opt)
{
	//printf("\nDestination: %s (%i)\nSource: %s (%i)\nOption: %c\n", dest, dest_len, src, strlen(src), opt);
	if (strlen(src) >= dest_len) {
		printf("Content of option -%c (%s) too long (>%i)!\n", opt, src, dest_len);
		exit(EXIT_FAILURE);
	} else {
		strncpy(dest, src, dest_len);
	}	
}

// *********************************************************************
// *********************************************************************
// *********************************************************************
int main(int argc, char **argv)
{
    int c;
    
    signal(SIGINT, exit_handler);
    signal(SIGKILL, exit_handler);
    signal(SIGHUP, exit_handler);
    signal(SIGQUIT, exit_handler);
    
    // ein Systemtimer
	signal(SIGALRM, alarm_callback);
	alarm(DISPLAY_REFRESH);

	// Kommandozeile auswerten
	while ((c=getopt(argc, argv, "h:p:u:P:q:r:t:T:l:L:m:M:s:S:a:A:v:V:d?")) != -1) {
		switch (c) {
			case 'h':
				check_copy_str_param(mqtt_host, sizeof mqtt_host, optarg, (char)c);
				break;
			case 'p':
				mqtt_port = atoi(optarg);
				break;
			case 'u':
				check_copy_str_param(mqtt_user, sizeof mqtt_user, optarg, (char)c);
				break;
			case 'P':
				check_copy_str_param(mqtt_pwd, sizeof mqtt_pwd, optarg, (char)c);
				break;
			case 'i':
				check_copy_str_param(mqtt_client_id, sizeof mqtt_client_id, optarg, (char)c);
				break;
			case 'r':
				display_refresh = atoi(optarg);
				break;
			case 'T':
				check_copy_str_param(temperature.key, sizeof temperature.key, optarg, (char)c);
				break;
			case 't':
				check_copy_str_param(temperature.value, sizeof temperature.value, optarg, (char)c);
				break;
			case 'L':
				check_copy_str_param(luminosity.key, sizeof luminosity.key, optarg, (char)c);
				break;
			case 'l':
				check_copy_str_param(luminosity.value, sizeof luminosity.value, optarg, (char)c);
				break;
			case 'M':
				check_copy_str_param(humidity.key, sizeof humidity.key, optarg, (char)c);
				break;
			case 'm':
				check_copy_str_param(humidity.value, sizeof humidity.value, optarg, (char)c);
				break;
			case 'S':
				check_copy_str_param(pressure.key, sizeof pressure.key, optarg, (char)c);
				break;
			case 's':
				check_copy_str_param(pressure.value, sizeof pressure.value, optarg, (char)c);
				break;
			case 'A':
				check_copy_str_param(awake_time.key, sizeof awake_time.key, optarg, (char)c);
				break;
			case 'a':
				check_copy_str_param(awake_time.value, sizeof awake_time.value, optarg, (char)c);
				break;
			case 'V':
				check_copy_str_param(vcc.key, sizeof vcc.key, optarg, (char)c);
				break;
			case 'v':
				check_copy_str_param(vcc.value, sizeof vcc.value, optarg, (char)c);
				break;
			case 'd':
				debug = 1;
				break;
			case '?':
				puts("epd_weather_monitor [-h <mqtt-host>] [-p <mqtt-port>]");
				puts("                    [-U <mqtt-user>] [-P <mqtt-pwd>]");
				puts("                    [-i <mqtt-id>]   [-r <display-refresh (sec)>]");   
				puts("                    [-T <JSON-key temperature] [-t <JSON-value temperature>]");
				puts("                    [-L <JSON-key luminosity]  [-l <JSON-value luminosity>]");
				puts("                    [-M <JSON-key humidity]    [-m <JSON-value humidity>]");
				puts("                    [-S <JSON-key pressure]    [-s <JSON-value pressure>]");
				puts("                    [-A <JSON-key awake_time]  [-a <JSON-value awake_time>]");
				puts("                    [-V <JSON-key Vcc]         [-v <JSON-value Vcc>]");
				puts("                    [-d]");
				exit(0);
				break;
		}
	}	

	// MQTT initialisieren; Topic abonnieren
    // Init Mosquitto-Lib...
    mosquitto_lib_init();
    // einen Mosquitto-Client erzeugen
    mosq = mosquitto_new(mqtt_client_id, true, NULL);
    if( mosq == NULL )
    {
        switch(errno){
            case ENOMEM:
                fprintf(stderr, "Error: Out of memory.\n");
                break;
            case EINVAL:
                fprintf(stderr, "Error: Invalid id and/or clean_session.\n");
                break;
        }
        mosquitto_lib_cleanup();
        exit(EXIT_FAILURE);
    }

	// MQTT-Callbacks definieren
    mosquitto_log_callback_set(mosq, my_log_callback);
	mosquitto_message_callback_set(mosq, my_message_callback);   
	mosquitto_connect_callback_set (mosq, my_connect_callback);
	mosquitto_disconnect_callback_set (mosq, my_disconnect_callback);
	// MQTT-User/Pwd setzen
	mosquitto_error_handling(mosquitto_username_pw_set(mosq, mqtt_user, mqtt_pwd));
	// mit MQTT-Broker verbinden
    mosquitto_error_handling(mosquitto_connect(mosq, mqtt_host, mqtt_port, MQTT_KEEPALIVE));

    // E-Paper initialisieren
	my_epd_init();
	
    // MQTT-Endlosloop
   	mosquitto_error_handling(mosquitto_loop_forever(mosq, -1, 1));
   	
	// ...eigentlich nie hierhin!
    my_epd_exit();
    return 0;
}
