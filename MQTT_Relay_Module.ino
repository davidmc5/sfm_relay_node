
#define FW_VERSION "1.20"

/* 
 * To compile and upload with Arduino IDE: 
 * set the board to "Generic ESP8266 Module"
 * CPU frequency 80 MH
 * Flash size 2MB / FS: 512KB / OTA: 768KB
 */
 

/*
 * To print messages to serial console:
 *    sprint(debug level, *msg1, *msg2);
 *    sprint(0, "ALERT Message",);
 *    sprint(1, "DEBUG Message",);
 *    sprint(2, "INFO message",);
 *    
 * DEBUG levels will print:
 *    0 = ALERT
 *    1 = ALERT & DEBUG
 *    2 = ALERT & DEBUG & INFO
*/
#define DEBUG 2 // Print all levels. Comment this out to disable console messages on production.

#define BAUD 9600 // Debug Serial baud rate.

/*
 * REFERENCES
 * 
 * The base of this code is from:
 * Arduino App > File > Examples > Examples for ESP8266> DNSServer > CaptivePortalAdvanced
 * 
 * To control the soft Access Point, including disconnect, see this tutorial:
 * https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/soft-access-point-class.html
 * 
 * Check for WIFI connectivity to the access point in the background
 * https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/generic-examples.html
 * 
 * Blink LED on a background timer
 * https://circuits4you.com/2018/01/02/esp8266-timer-ticker-example/
 * 
 * Set up a background timer to disconnect AP 30 seconds after power up if no clients are connected
 * https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/soft-access-point-class.html#softapdisconnect
 * 
 * Set up MQTT client
 * https://techtutorialsx.com/2017/04/09/esp8266-connecting-to-mqtt-broker/
 */



#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <ESP8266httpUpdate.h> //fota
#include <stdio.h> //sprintf()

#include "utils.h"
#include "mqtt_brokers.h"

/*
 * STEPS
 * 
 * CHANGE CONST DEFINITIONS TO #DEFINE STATEMENTS

 * DONE: This example serves a "hello world" on a WLAN and a SoftAP at the same time. The SoftAP allowS you to configure WLAN (client) parameters at run time. 
         They are not setup in the sketch but saved on EEPROM.

 * DONE: Connect to AP with computer or cell phone to wifi network ESP_ap with password 12345678. A popup may appear with a link to WLAN config. If it does not then navigate to http://192.168.4.1/wifi and config it there.
         Then wait for the module to connect to your wifi and take note of the WLAN IP it got. Then you can disconnect from ESP_ap and return to your regular WLAN.

 * DONE: Now the ESP8266 is in your network. You can reach it through http://192.168.x.x/ (the IP you took note of) or maybe at http://esp8266.local too.

////////////////////////////////
 * DONE: THIS DOES NOT WORK IF THE WIFI CLIENT IS NOT SET YET, ON A NEW DEVICE!
 *  This is a captive portal because through the softAP it will redirect any http request to http://192.168.4.1/ 
 * 
 * 
 * TASK-1: 
         -Turn the onboard LED on power on


 * TASK-2: 
 *       -Turn LED on when it connects to AP ands gets an IP
 *       
 *       TODO:
 *       - flash LED while not connected to AP and then solid when connected 
 *       
 *       
 *       
 * TASK-3 
 *      - Blink LED using background timer      
 *      
 * TASK-4
 *      - set up a timer to turn off led 30 seconds after connected to AP
 *      - Use the same timer to disconnect AP 30 seconds after connected to AP
 *      
 * TASK-5
 *      - Add MQTT client
 *      
 *      
 *      
 * TASK-6
 *      - Test I2C with I/O expander IC MCP23017
 *      - #include <Wire.h>
 *      
 *      
 *      
 * TODO: 
 *      - PUT THE SOFT-AP CODE AS A FUNCTION IN A SEPARATE FOLDER

         
*/

///////////////////////////////////////////////////
////// PUT ALL THESE DEFINITIONS ON config/header FILES 
///////////////////////////////////////////////////

/*
 * SoftAP credentials for config captive portal
 * if an AP password is defined it needs to be 8 characters or more
 * if no AP password is defined, a client will login automatically after selecting the SSID.
 */
#ifndef APSSID
#define APSSID "RLY_Node_" /* the ap ssid will include the last 4 digits of the MAC: SFM_Node_B033 */
#define APPSK  "" /* Do not set a password */
#endif

char softAP_ssid[20];
const char *softAP_password = APPSK;
/* hostname for mDNS. Should work at least on windows. Try http://node.local */
const char *myHostname = "node";

/* levels for debug messages */
char *debugLevel[] = {"ALERT", "DEBUG", "INFO"};

/*
 * WiFi Connection States:
 * -1: Timeout //NOTE: THIS RETURN WILL BREAK THE ARRAY LOOKUP wifiStates DEFINED BELOW! NEED TO FIX! Use enum?
 * 
 * 0 : WL_IDLE_STATUS when Wi-Fi is in process of changing between statuses
 * 1 : WL_NO_SSID_AVAILin case configured SSID cannot be reached
 * 3 : WL_CONNECTED after successful connection is established
 * 4 : WL_CONNECT_FAILED if password is incorrect
 * 6 : WL_DISCONNECTED if module is not configured in station mode
 */
char *wifiStates[] = {"IDLE", "NO SSID", "2:?", "CONNECTED", "INCORRECT PASSWORD", "5:?", "DISCONNECTED"};
unsigned int wifiStatus = WL_IDLE_STATUS; /* Set initial WiFi status to 'Not connected-Changing between status */


/* 
 *  Configuration Settings
 *  Max flash: 512 bytes
 *  Use macro SAVECFG(setting)to save a setting to flash (eeprom_utils)
 *  
 *  ITEMS TO ADD:
 *  ADDRESS TO GET PUBLIC IP
 *  ERROR COUNTERS TO DETECT LOOPS
 *  
 *  Previous settings:
 *  
 *  struct cfgSettings_s{  
 *    char ap_ssid[32];
 *    char ap_pswd[32];
 *    char ap_ok[3];
 */
#define cfgStartAddr 0
struct cfgSettings_s{  
  char firstRun[3]; /* Flag to detect very first boot of a new device: unitialized flash memory has garbage */
  char ap_ssid[20];
  char ap_pswd[20];
  char site_id[10]; /* Populated by initial setup or HELLO handshake */
  /* debug log flag */
  char debug[5]; //s=serial, m=mqtt: example s0m2 = send 'alerts' to serial and 'all' to mqtt
  /* node type */
  char node_type[20]; /* r-16;s-16 */
  /* mqtt servers */
  char mqttServerA[20];
  char mqttPortA[5];
  char mqttUserA[20];
  char mqttPasswordA[20];
  char mqttServerB[20];
  char mqttPortB[5];
  char mqttUserB[20];
  char mqttPasswordB[20];  
} cfgSettings;



/* Node's info */
char nodeId[18]; /* wifi MAC - this is the nodeId variable populated by the wifi module */
char wanIp[20]="0.0.0.0"; /* public IP of the node. Used by controller to determine siteId on hello/ mqtt message */
//char siteId[10]= "NEW_NODE"; //STORED IN flash - unique ID used as mqtt topic for all site nodes. Populated by initial setup or HELLO handshake.



// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Web server
ESP8266WebServer server(80); ////RENAME 'server' TO 'HTTPServer' TO BE MORE CLEAR WHAT SERVER THAT IS

/* Soft AP / captive portal network settings */
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);


/** Connect to WLAN? */
boolean connect;

/** Last time connected to WLAN */
unsigned long lastConnectTry = 0;


/* mqtt settings */
#define mqttMaxTopicLength 50
#define maxTopics 10
#define mqttMaxPayloadLength 100
#define mqttClientPrefix "NODE-"
char mqttTopic[mqttMaxTopicLength];
char *topics[maxTopics]; /* this array holds the pointers to each topic token. */
char rawTopic[mqttMaxTopicLength]; /* will have the same string as the given 'topic' buffer but with '/' replaced with '/0' */
char mqttPayload[mqttMaxPayloadLength];
char mqttClientId[20]; /* Unique client ID (clientPrefix+MAC) to connect to mqtt broker. Used by the mqtt module: mqttClient.connect()*/ 
uint8_t mqttPriFlag = 1; /* Primary mqtt broker status flag. 1=failed / 0 = OK */
uint8_t mqttBakFlag = 1; /* Backup mqtt broker status flag. 1=failed / 0 = OK */
uint8_t mqttBrokerState = 0; /* state machine for sending updates -- see mqtt module */


char tempBuffer[mqttMaxPayloadLength]; /*used for strcat/strcpy /payload and temp storage - handleHttp and mqtt modules */

/** I2C Relay Control */
#define I2C_ADDR 0x27 /*Address of I2C IO expander */
#define SDA 2 /* ESP-WROOM uses gpio 2 for i2c SDA */
#define SCL 14 /* ESP-WROOM uses gpio 14 for i2c SCL */
//#define SDA 4 /* WEMOS uses gpio 4 for i2c SDA */
//#define SCL 5 /* WEMOS uses gpio 5 for i2c SCL */
//16 bits to keep track of on/off state for 16 relays.
//uint16_t relayState = 0x0000; //all relays on
uint16_t relayState = 0xFFFF; //all relays off



////TASK-1:
/** Control onboard LED */
//const int LED = 2; //GPIO-2 onboard LED WEMOS-mini pro
const int LED = 12; //GPIO-12 ESP-WROOM - PWM pin

const int LED_ON = 0; //active low
const int LED_OFF = 1;


////// PUT ALL THESE FUNCTION DEFINITIONS ON SEPARATE FILES AND SET DEBUG TRAP
void setLED(int led, int action) {                          
  digitalWrite(led, action);      
  //digitalWrite(led,!digitalRead(led));   // Change the state of the LED
}


////TASK-2
/* Declare the wifi event handlers */
// will get called when it gets an IP. Used to turn on LED (stop blinking)
WiFiEventHandler gotIpEventHandler;

/* will be called when disconnected from wifi - to start led blinking */
WiFiEventHandler disconnectedEventHandler;

////TASK-3
//blink led using timer (include <Ticker.h>)
Ticker blinker;
void changeState(){
  digitalWrite(LED, !(digitalRead(LED))); //invert current led state
}

////TASK-4
//user timer to switch off AP after a time delay
Ticker APtimer;

void turnAPoff(){
  //turn off AP only if no clients are connected
  if (WiFi.softAPgetStationNum() == 0){
    WiFi.softAPdisconnect(true);
    APtimer.detach();
    sprint(2, "Access Point Disabled", );    
  }
}

//to test time delay function
//void turnLEDoff(){
//  APtimer.detach();
//  setLED(LED, LED_OFF);
//}

////TASK-5
// Set up mqtt client
//needs <PubSubClient.h>
//Reference for multiple brokers:
//https://github.com/knolleary/pubsubclient/issues/511

// Create a client that can connect to a specified internet IP address and port as defined in client.connect()
// see mqtt.ino module for mqttClient.connect()
// see setup() function below for mqttClient.setServer()
WiFiClient espClient1; //Primary mqtt broker
WiFiClient espClient2; //Backup mqtt broker
PubSubClient mqttClient1(espClient1);
PubSubClient mqttClient2(espClient2);


//////////////////
/* SETUP */
/////////////////
void setup() {

  delay(1000); //not sure if this delay is needed

  /* Intitialize serial port to print console mesages if DEBUG mode is set */
  serialInit();

  //print Restart Message
  //If no previous crash it will report: "External System" or "Power On"
  //After a software upgrade: "Software/System restart"
  sprint(0,"BOOTING", ESP.getResetInfo());
  sprint(0,"FIRMWARE", FW_VERSION);

  /* Do not store wifi settings in flash */  
  WiFi.persistent(false);

/////////////////////////////////////
  fixSettings();

////////////////////////////////////

  
  
  ////TASK-1:
  pinMode(LED, OUTPUT); //configure the led pin as an output.
  //outputs appear to be at zero on start, driving the LED on.
  //setLED(LED, LED_OFF);

  
  ////TASK-2
  ///////////////////// 
  //https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/generic-examples.html
  //https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/generic-class.html
  //Turn LED ON when client adquires IP from AP
  //NOTE: DISABLED SINCE WE ARE NOW TRUNING THE LED ONLY WNEN NODE IS CONNECTED TO AT LEAST ONE MQTT BROKER
//  gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event){
//    blinker.detach();
//    setLED(LED, LED_ON);
//  });

  /// blink led when it loses wifi
  disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event){
    blinker.attach(0.5, changeState);   
  });
    
  ////TASK-3
  // blink LED
  //Set a ticker every 0.5s
  blinker.attach(0.5, changeState);//Time is in seconds. Use attach_ms if you need time in ms

  ////TASK-4
  //Set a 60 sec timer to switch off AP
  APtimer.attach(60, turnAPoff);

  ////TASK-5
  //Set MQTT servers (mqttCallback function (just one for both brokers) is declared on mqtt.ino module
  mqttClient1.setServer(mqttServer1, mqttPort1);  //Primary mqtt broker
  mqttClient1.setCallback(mqttCallbackPri); //function executed when a MQTT message is received.
  mqttClient2.setServer(mqttServer2, mqttPort2); //Backup mqtt broker
  mqttClient2.setCallback(mqttCallbackBak); //function executed when a MQTT message is received.


  
  // start softAP
  startAP();


////TASK-6 i2c

  i2cInit(&relayState); /* Initialize I2C */

  /* For testing - Send on/off commands to relays */
  //  setOutput(&relayState, "1", 1); // turn relay 1 on
  //  setOutput(&relayState, "1", 0); // trun relay 1 off

//////////////////////
  manageMqtt();
} /* END OF SETUP */


//////////////////
/* LOOP*/
/////////////////

void loop() {
  
  manageWifi();


  //DNS
  dnsServer.processNextRequest();


  ////////
  /// RENAME server TO httpServer
  ////////////////////////////////
  //HTTP
  server.handleClient();
  MDNS.update();

  manageMqtt();
 
} /* END OF LOOP */
