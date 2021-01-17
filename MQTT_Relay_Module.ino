
#define FW_VERSION "1.43"
/*
 *  COMMIT NOTES
 *  
 *  Clean startSoftAP() and loop() calls
 *  
 *  
 *  TO DO NEXT:
 *  
 *  MAKE A void connectWlan() TO TEST LAST/A/B/FACT IN SEQUENCE TO FIND ONE THAT WORKS
 *  START FAST BLINKING (SEARCHING AN AP)
 *  IF A WORKING AP IS FOUND, STORE IN LAST AND SET LED ON
 *  IF NOT, ENABLE SOFTAP AND SET SLOW BLINK (NO VALID AP FOUND) AND WAIT 1 MINUTE FOR CLIENT CONNECTION
 *  IF A NEW AP IS CONFIGURED AND WORKS, STORE TO LAST.
 *  
 *  
 *  ON BOOT, FLASH LED AND ATTEMPT FIRST TO CONNECT TO STORED APs, WITH SOFTAP DISABLED
 *  IF UNABLE TO CONNECT, ENABLE SOFTAP AND FAST-BLINK THE LED
 *  AFTER ONE MINUTE, IF NO SOFTWAP CLIENTS CONNECTED, DISABLE SOFTAP AND SLOW-BLINK THE LED
 *  
 *  WHEN THE NODE CONNECTS TO MQTT BROKER, FORCE DISCONNECT ALL CLIENTS AND DISABLE SOFTAP
 *  FURTHER CONFIGURATION WILL BE DONE VIA MQTT
 *  
 *  
 * COMPILER - HARDWARE SETTINGS
   * To compile and upload with Arduino IDE: 
   * set the board to "Generic ESP8266 Module"
   * CPU frequency 80 MH
   * Flash size 2MB / FS: 512KB / OTA: 768KB
 * 
 * 
 * 
 * DEBUG SETTINGS
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
 *    
 *    
 * TODO:
 * change DEBUG so it prints selectively. i.e., DEBUG 0,2 (print only alert and debug)
 *    
*/
#define DEBUG 2 //Comment this out to disable console messages on production.

#define BAUD 9600
//#define BAUD 115200

/*
 * REFERENCES
 * 
 * Arduino App > File > Examples > Examples for ESP8266> DNSServer > CaptivePortalAdvanced
 * 
 * espressif SDK
 * https://www.espressif.com/sites/default/files/documentation/2c-esp8266_non_os_sdk_api_reference_en.pdf
 * 
 * ESP8266/Arduino Releases: 
  * https://github.com/esp8266/Arduino/releases
 * 
 * ESP8266 Arduino Core Documentation:
   * https://buildmedia.readthedocs.org/media/pdf/arduino-esp8266/docs_to_readthedocs/arduino-esp8266.pdf
   * https://arduino-esp8266.readthedocs.io/en/latest/
   * https://arduino-esp8266.readthedocs.io/en/latest/libraries.html
   * https://github.com/esp8266/Arduino/tree/master/libraries
   * https://github.com/esp8266/Arduino/blob/master/tools/sdk/include/c_types.h
   * https://github.com/esp8266/Arduino/blob/master/cores/esp8266/core_esp8266_features.h
 * 
 * WIFI connectivity to an access point
 * https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/readme.html
   * https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/station-class.html
   * https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/generic-examples.html
 * 
 * Webserver: Accessing the body of a HTTP request
  * https://techtutorialsx.com/2017/03/26/esp8266-webserver-accessing-the-body-of-a-http-request/
 * 
 * To control the soft Access Point, including disconnect, see this tutorial:
  * https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/soft-access-point-class.html
 * 
 * Create A Simple ESP8266 Web Server In Arduino IDE
   * https://lastminuteengineers.com/creating-esp8266-web-server-arduino-ide/
 * 
 * Blink LED on a background timer
   * https://circuits4you.com/2018/01/02/esp8266-timer-ticker-example/
 * 
 * Set up a background timer to disconnect AP 30 seconds after power up if no clients are connected
  * https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/soft-access-point-class.html#softapdisconnect
 * 
 * Set up MQTT client
   * https://techtutorialsx.com/2017/04/09/esp8266-connecting-to-mqtt-broker/
 * 
 * Ticker with arguments
   * https://github.com/esp8266/Arduino/blob/master/libraries/Ticker/examples/TickerParameter/TickerParameter.ino
 */



#include <ESP8266WiFi.h> /* raw access to wifi independent of protocol - https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/readme.html */
#include <WiFiClient.h> /* to send and receive data to a server but you have to format the data yourself e.g. message headers and parsing the response */
#include <ESP8266HTTPClient.h> /* to communicate with a http web server and does all the formatting and parsing needed */

#include <ESP8266WebServer.h> //https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer
#include <DNSServer.h> //https://github.com/esp8266/Arduino/tree/master/libraries/DNSServer
#include <ESP8266httpUpdate.h> //fota

#include <EEPROM.h>
#include <Ticker.h> //https://www.arduino.cc/reference/en/libraries/ticker/
#include <PubSubClient.h>
#include <Wire.h>
#include <stdio.h> //sprintf()

#include "utils.h"
#include "mqtt_brokers.h"
#include "settings.h"

// This is already included in some other .h file:
//extern "C" {
//  #include<user_interface.h>
//}

/*
 * Function Prototypes
 * 
 * Note: Most code is written in ANSI C but using C++ style default function arguments. 
 * C does not allow default/optional function arguments
 */
void sendMqttMsg(char * mqttTopic, char *mqttPayload, bool retain=false);
void configSettings(char *setting, char *value="");
void getCfgSettings(struct cfgSettings_s *structSettings = &cfgSettings);
void publishSetting(char *setting="all");
void publishError(char *setting="ERROR");


/*
 * SoftAP credentials for config captive portal
 * if an AP password is defined it needs to be 8 characters or more
 * if no AP password is defined, a client will login automatically after selecting the SSID.
 */
#ifndef APSSID
#define APSSID "RLY_Node_" /* the ap ssid will include the last 4 digits of the MAC: SFM_Node_B033 */
#define APPSK  "" /* Do not set a password */
#endif

char softAP_ssid[20]; /* ssid broadcast by the softAP for clients to connect to and change config settings */
const char *softAP_password = APPSK;

/* levels for debug messages */
char *debugLevel[] = {"ALERT", "DEBUG", "INFO"};

/*
 * WiFi Connection States:
 * -1: Timeout //NOTE: THIS RETURN WILL BREAK THE ARRAY LOOKUP wifiStates DEFINED BELOW! NEED TO FIX! Use enum starting at -1?
 * 
 * 0 : WL_IDLE_STATUS when Wi-Fi is in process of changing between statuses
 * 1 : WL_NO_SSID_AVAILin case configured SSID cannot be reached
 * 3 : WL_CONNECTED after successful connection is established
 * 4 : WL_CONNECT_FAILED if password is incorrect
 * 6 : WL_DISCONNECTED if module is not configured in station mode
 */
char *wifiStates[] = {"IDLE", "NO SSID", "2:?", "CONNECTED", "INCORRECT PASSWORD", "5:?", "DISCONNECTED"};
unsigned int prevWifiStationState = WL_IDLE_STATUS; /* Set initial WiFi status to 'Not connected-Changing between status */
//unsigned int currentWifiState;

/* Node's info */
char nodeId[18]; /* wifi MAC - this is the nodeId variable populated by the wifi module */
char wanIp[20]="0.0.0.0"; /* public IP of the node. Used by controller to determine siteId on hello/ mqtt message */
//char siteId[10]= "NEW_NODE"; //STORED IN flash - unique ID used as mqtt topic for all site nodes. Populated by initial setup or HELLO handshake.

/* Web Server */
ESP8266WebServer httpServer(80); /* Create a webserver object that listens for HTTP request on port 80 */
void handleRoot();              // function prototypes for HTTP handlers
void handleNotFound();
int wifiScannedIps; /////////number of access points found... RENAME TO wifiScannedAPs

/* Soft AP / captive portal network settings */
IPAddress apIP(10, 1, 1, 1); 
IPAddress netMsk(255, 255, 255, 0);

/* DNS server */
const byte DNS_PORT = 53;
DNSServer dnsServer;


/* MQTT settings */
#define mqttMaxTopicLength 50
#define maxTopics 10
#define mqttMaxPayloadLength 100
#define mqttClientPrefix "NODE-"
char mqttTopic[mqttMaxTopicLength];
char *topics[maxTopics]; /* this array holds the pointers to each topic token. */
char rawTopic[mqttMaxTopicLength]; /* will have the same string as the given 'topic' buffer but with '/' replaced with '/0' */
char mqttPayload[mqttMaxPayloadLength];
char mqttClientId[20]; /* Unique client ID (clientPrefix+MAC) to connect to mqtt broker. Used by the mqtt module: mqttClient.connect()*/ 

bool mqttBrokerUpA = false; /* Primary mqtt broker status flag */
bool mqttBrokerUpB = false; /* Backup mqtt broker status flag */

uint8_t mqttBrokerState = 0; /* state machine for sending updates -- see mqtt module */
uint8_t mqttErrorCounter = 0; /* used to reset mqtt broker settings to defaults */

#define ERROR -1

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


/*
 * LED Control
 * blink = not connected to mqtt broker
 * solid = connected to mqtt broker
 */
//const int LED = 2; //GPIO-2 onboard LED WEMOS-mini pro
const int LED = 12; //GPIO-12 ESP-WROOM - PWM pin
const int LED_ON = 0; //active low
const int LED_OFF = 1;
void setLED(int led, int action) { 
  /*                         
   * action options: LED_ON, LED_OFF                      
   */
  digitalWrite(led, action);      
  //digitalWrite(led,!digitalRead(led));   // Change the state of the LED
}


/*
 * FLAGS
 * 
 * Used for ticker and wifi event callbacks
 * Do not place blocking functions inside callbacks
 * Set instead a flag and check it inside loop()
 */

/* flags set by the WIFI callback functions */
bool wifiStatusChange = false;

bool wifiStationConnected = false;
bool wifiStationDisconnected = false;
bool wifiReconnectAttempt = false; /* used to ignore wifiStationDisconnected event when attempting to reconnect */

bool wifiStationGotIp = false;
bool wifiSoftApClientGotIp = false;
bool wifiSoftApClientConnected = false;
bool wifiSoftApClientDisconnected = false;
char clientMac[18] = {0};

/* 
 *  TIMERS
 */
Ticker blinkLedTimer; /* Timer to blink LED - calls blinkLed() - usage: blinkLedTimer.attach(0.5, blinkLed); on/off for 0.5 second */
void blinkLed(){
  digitalWrite(LED, !(digitalRead(LED))); //invert current led state
}
void ledBlink(){
  blinkLedTimer.attach(0.5, blinkLed);
}
void ledOn(){
  blinkLedTimer.detach();
  setLED(LED, LED_ON);
}
  

/* Timer to disable softAP - Example to disable sofAP in 60 sec: softApTimer.attach(60, disableSoftAp); */
Ticker softApTimer; 
bool softApUp = false;
void disableSoftAp(){
  /* only disable softAP if there are no clients connected */
  /////////////////////////////////////////////////////////
  //TODO: DISCONNECT CLIENTS AND STOP DNS AND HTTP SERVERS
  ///////////////////////////////////////////////////////////
  if (WiFi.softAPgetStationNum() == 0){                  
    WiFi.softAPdisconnect(true);
    softApUp = false;
    wifiStatusChange = true;
    softApTimer.detach();
  }
}

/* Timer to retry wifi connectivity - calls retryWifi() */
Ticker retryWifiTimer;
bool retryWifiFlag = true;
void retryWifi(){
  /*
   * This function is called by the retryWifiTimer to set the retryWifiFlag
   * When retryWifiFlag is set, the main loop() will disconnect station-mode from current AP
   * and attempt to connect to the given AP and restart the retryWifiTimer.
   */
  retryWifiFlag = true;
  retryWifiTimer.detach();
}


/*
 * CALLBACK EVENT HANDLERS 
 * 
 * https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/generic-class.html#wifieventhandler
 * https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/ESP8266WiFiGeneric.cpp
 */

/*
 * triggers when station mode connects to an AP but before wifi is fully up (before station node gets an IP)
 * this event is only used for diagnostics
 */
WiFiEventHandler stationConnectedHandler;
void onStationConnected(const WiFiEventStationModeConnected event){
  wifiStationConnected = true;
  wifiStatusChange = true;
  //sprint(1, "-------------> Event StationModeConnected - WIFI IS STILL DOWN: ", WiFi.isConnected());
}

/* triggers when the device (in station mode) disconnects from the AP */
WiFiEventHandler stationDisconnectedtHandler;
void onStationDisconnected(const WiFiEventStationModeDisconnected event){
  if (!wifiReconnectAttempt){ /* Only flag unexpected events, not when we are forcing a disconnect to connect to a differnt AP */
    wifiStationDisconnected = true;
    wifiStatusChange = true;
  }
  wifiReconnectAttempt = false;
  internetUp = false;
}

/*
 * triggers when the NODE/STATION gets an IP from the Access Point
 * This event sets the systeme flag WiFi.isConnected()== true
 * This event is only used for diagnostics
 */
WiFiEventHandler stationGotIpHandler;
void onStationGotIp(const WiFiEventStationModeGotIP event){
  wifiStationGotIp = true;
  wifiStatusChange = true;  
  if(WiFi.isConnected()){ /* This should always be true! With an IP assigned, wifi whould be up */
    retryWifiTimer.detach(); /* Stop retrying connecting to wifi - Connected with an IP! */ 
    retryWifiFlag = false; /* prevent disconneting and retrying station wifi! */
  }
  /* Below print commands are for test only - Do not leave blocking commands on a callback! */
//  sprint(1, "STATION IP: ", event.ip); 
//  sprint(1, "-------------> Event StationModeGotIP - WIFI IS UP: ", WiFi.isConnected());
}

/* triggers when a device connects to the softAP. Event has MAC of the device */
WiFiEventHandler softApClientConnected;
void onSoftAPclientConnected(const WiFiEventSoftAPModeStationConnected event){
  wifiSoftApClientConnected = true;
  wifiStatusChange = true;
  /* get MAC of client that just connected to softAP */
  snprintf(clientMac, sizeof(clientMac), MACSTR, MAC2STR(event.mac));
}
 
/* triggers when a device disconnects from the softAP */
WiFiEventHandler softApClientDisconnected;
void onSoftAPclientDisconnected(const WiFiEventSoftAPModeStationDisconnected event){
  wifiSoftApClientDisconnected = true;
  wifiStatusChange = true;
  /* get MAC of client that just disconnected from softAP */
  snprintf(clientMac, sizeof(clientMac), MACSTR, MAC2STR(event.mac));
}
/*
 * Definitions to copy a mac address (in array locations 0-5) to a string
 * #define   MACSTR   "%02x:%02x:%02x:%02x:%02x:%02x"
 * #define   MAC2STR(a)   (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
 */  
  
/* 
 *  Callback for all wifi events
   *  But only used for event:
   *   WIFI_EVENT_SOFTAPMODE_DISTRIBUTE_STA_IP
   *  
 *  It uses WiFi.onEvent which is DEPRECATED!!!
   *  STILL INCLUDED IN THE LATEST ESP8266WiFiGeneric.cpp
   *  https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/ESP8266WiFiGeneric.h#L63
   *  https://github.com/godstale/ESP8266_Arduino_IDE_Example/blob/master/example/WiFiClientEvents/WiFiClientEvents.ino
   *  
 *  TRY ADDING THE 'DISTRIBUTE_STA_IP' EVENT IN THIS LIBRARY:
   *  https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/ESP8266WiFiGeneric.cpp#L94
   *  It seems that this event WIFI_EVENT_SOFTAPMODE_DISTRIBUTE_STA_IP
   *  triggers when an IP is assigned to a softAP client (Need to confirm)
   *  
 *  This callback is registered in setup() with the line:
 *  WiFi.onEvent(softApDistributeIpEvent, WIFI_EVENT_SOFTAPMODE_DISTRIBUTE_STA_IP);
   *  https://techtutorialsx.com/2019/08/11/esp32-arduino-getting-started-with-wifi-events/
   *  https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/ESP8266WiFiType.h
   */
void softApDistributeIpEvent(WiFiEvent_t event) {
  wifiSoftApClientGotIp = true;
  wifiStatusChange = true;
}

/*
 * 
 *  Set up mqtt client
 *  
 * include <PubSubClient.h>
 * Reference for multiple brokers:
 * https://github.com/knolleary/pubsubclient/issues/511
 *  Create a client that can connect to a specified internet IP address and port as defined in client.connect()
 *  see mqtt.ino module for mqttClient.connect()
 *  see setup()/ mqttClient.setServer()
 */
WiFiClient espClientA; //Primary mqtt broker
WiFiClient espClientB; //Backup mqtt broker
PubSubClient mqttClientA(espClientA);
PubSubClient mqttClientB(espClientB);

uint32 prevFreeHeap = 0; 
void showFreeHeapOnChange(){
  /* 
   * Monitors memory leaks
   * Display a message when the free heap varies more than 2000
   * https://github.com/esp8266/Arduino/blob/master/cores/esp8266/Esp.cpp
   * uint32_t EspClass::getFreeHeap(void)
   */
  if ( prevFreeHeap > ESP.getFreeHeap()+2000 ){
    sprint(0, "FREE HEAP DECREASED TO: ", ESP.getFreeHeap()); 
  }else{  
    if ( prevFreeHeap + 2000 < ESP.getFreeHeap()){
      sprint(2, "FREE HEAP INCREASED TO: ", ESP.getFreeHeap());
    }
  }
  if (ESP.getFreeHeap() < 20000){
    sprint(0, "FREE HEAP IS BELOW 50%. POSSIBLE MEMORY LEAK! - ", ESP.getFreeHeap());
  }
  prevFreeHeap = ESP.getFreeHeap();  
}

void monitorEvents(){
  /* print status changes */
  if (wifiStatusChange){
    wifiStatusChange = false;
    sprint(2, "",); /* add a blank line separator for easy reading */
    sprint(2, "WIFI EVENTS --------------------", );
    if(wifiSoftApClientConnected){
      wifiSoftApClientConnected = false;
      sprint(2, "EVENT: SOFTAP CLIENT CONNECTED. MAC: ", clientMac);
    }
    if(wifiSoftApClientDisconnected){
      wifiSoftApClientDisconnected = false;      
      sprint(2, "EVENT: SOFTAP CLIENT DISCONNECTED. MAC: ", clientMac);
      showSoftApClients();
    }
    if (wifiSoftApClientGotIp){ 
      sprint(2, "EVENT: SOFTAP CLIENT GOT IP",);
      wifiSoftApClientGotIp = false; /* reset event flag */
      showSoftApClients(); /* display the clients connected to the softAP if an IP assignment event got triggered */
    }
    if(wifiStationConnected){ /* this event is only used for diagnostics */
      wifiStationConnected = false;
      sprint(2, "EVENT: WIFI STATION-MODE CONNECTED (No IP assigned yet!)",);
    }
    if (wifiStationGotIp){ /* this event is only used for diagnostics */
      wifiStationGotIp = false;
      sprint(2, "EVENT: WIFI STATION-MODE CONNECTED - IP: ", WiFi.localIP());
    }
    if(wifiStationDisconnected){
      wifiStationDisconnected = false;
      strcpy(wanIp, "0.0.0.0"); /* Clear WAN IP */
      sprint(0, "EVENT: WIFI STATION DISCONNECTED!",);
    }   
    sprint(2, "NODE STATUS ---------------------", );
    if(WiFi.isConnected()){
      sprint(2, "WIFI UP - LOCAL IP: ", WiFi.localIP());
    }else{
      sprint(0, "WIFI DOWN!",);
    }    
    if(internetUp){
      sprint(2, "INTERNET UP - PUBLIC IP: ", wanIp);
    }else{
      sprint(0, "INTERNET DOWN",);
    }
    if(mqttBrokerUpA || mqttBrokerUpB){ /* at least one broker is up */
      ledOn();
      sprint(2, "MQTT UP: ", mqttBrokerUpA + mqttBrokerUpB);
    }else{
      ledBlink();
      sprint(0, "MQTT BROKERS DOWN",);
    }    
    if(softApUp){
      sprint(2, "SOFTAP POINT ENABLED. Connected Clients: ", WiFi.softAPgetStationNum());
    }else{
      sprint(2, "SOFTAP POINT DISABLED", );
    }
    sprint(2,"Heap Fragmentation (< 50% is good): ", ESP.getHeapFragmentation());  /* 0% is clean. 50% OK */
    if (ESP.getHeapFragmentation() > 50){
      sprint(0, "HEAP FRAGMENTATION IS HIGH: ", ESP.getHeapFragmentation());
    }
  }
  showFreeHeapOnChange();
}


 
 
/***********************************************************/
/***********************************************************/
/***********************************************************/
/* SETUP */
/***********************************************************/
/***********************************************************/
/***********************************************************/


void setup() {

  /* Intitialize serial port to print console mesages if DEBUG mode is set */
  serialInit();

  /*
   * print Restart Message on boot
   * 
   * If no previous crash it will report: "External System" or "Power On"
   * After a software upgrade: "Software/System restart"
   */
  sprint(2, "BOOTING: ", ESP.getResetInfo());
  sprint(2, "FIRMWARE: ", FW_VERSION);
  sprint(2, "ESP CHIP ID: ", ESP.getChipId());
  sprint(2, "Core Version: ", ESP.getCoreVersion());
  sprint(2, "SDK Version: ", ESP.getSdkVersion());
  if (!ESP.checkFlashCRC()){
    sprint(0, "FLASH CORRUPTED!",);
  }  

  WiFi.persistent(false);   /* Do not store wifi settings in flash - this is handled by handleHttp */ 
  
  strcpy(cfgSettings.firstRun, "OK"); /* Assume flash data is OK on boot only to determine if they are valid */
  getCfgSettings();  /* load configuration settings from flash to ram */  
  /*
   * If 'firstRun' is not 'OK' then revert to factory defaults for wifi settings and mqtt brokers
   * Clear wifi ssid/pswd invalid settings if first time (garbage in flash)
   * 
   * /////////////////////////////
   * WE MIGHT NOT NEED THIS IF THE connectWlan FUNCTION TAKES CARE OF FINDING A WORKING AP
   */
  if ( strcmp(cfgSettings.firstRun, "OK") != 0){
    sprint(0,"FLASH DATA INVALID - LOADING DEFAULTS",);
    /* set wifi AP and mqtt brokers to factory defaults */
    strcpy(cfgSettings.apSSIDlast, apSSIDfact);
    strcpy(cfgSettings.apPSWDlast, apPSWDfact);
    loadMqttBrokerDefaults();
  }


 ////////vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
 // CHANGE apSSID & apPSWD
 
  /* For testing: store factory wifi AP into first LIFO location */
//  strcpy(wifiAPs[0].apSsid, apSSIDfact);
//  strcpy(wifiAPs[0].apPswd, apPSWDfact);

  //// FOR TESTING WITH AN INVALID AP ON BOOT
//  strcpy(cfgSettings.apSSIDa, "bad-ssid");
//  strcpy(cfgSettings.apPSWDa, "bad-pswd");
//  sprint(2,"LOADED BAD WIFI SETTINGS (ON SETUP) FOR TESTING !!!!!!!!!!!!!!!!!!!!!!!!",);

/////////////////////////////////////
//  fixSettings(); //remove after this release 1.22
////////////////////////////////////

//  /* Display stored wifi APs on LIFO */
//  sprint(2, "LIFO APs - Last: ",lastWifiAp);
//  for(int ap = 0; ap < maxWifiAps; ap++){
//    sprint(2, wifiAPs[ap].apSsid, wifiAPs[ap].apPswd);
//  }
 //////////^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 
  pinMode(LED, OUTPUT); /* configure the led pin as an output */
  //outputs appear to be at zero on start, driving the LED on.
  //setLED(LED, LED_OFF);

  
  /* 
   *  Blink led when node disconnects from WiFi AP (loses wifi signal, etc)
   *  
   *  See Event Driven Methods
   *  https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/generic-examples.html
   *  https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/generic-class.html
   *  https://roei.stream/2018/01/21/connecting-esp8266-microcontroller-to-a-wifi-network/
   *  
   *  To use, register event with a specific action
   *  1) Select the particular event (onStationModeDisconnected) and 
   *  2) add the code to be executed when event is fired.
   *  // We can't use "delay" or other blocking functions in the event handler.
   *  // Therefore we set a flag here and then check it inside "loop" function.
   */


  /* 
   *  Register Callback Functions with their corresponding events
   *  Callback functions will be called as long as their handlers exist
   */
  stationConnectedHandler =  WiFi.onStationModeConnected(&onStationConnected); /* station connected to an AP */
  stationDisconnectedtHandler = WiFi.onStationModeDisconnected(&onStationDisconnected); /* station disconnected from the AP */
  stationGotIpHandler = WiFi.onStationModeGotIP(&onStationGotIp); /* station got IP from AP */
  softApClientConnected = WiFi.onSoftAPModeStationConnected(&onSoftAPclientConnected); /* a device connectected to softAP */
  softApClientDisconnected = WiFi.onSoftAPModeStationDisconnected(&onSoftAPclientDisconnected); /* a device disconnected from softAP */
  /*
   * Register the callback function softApDistributeIpEvent for wifi event, WIFI_EVENT_SOFTAPMODE_DISTRIBUTE_STA_IP
   * 
   *  APARENTLY THE WiFi.onEvent() FUNTION IS DEPRECATED!
   *  https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/ESP8266WiFiGeneric.h#L64
   */
  WiFi.onEvent(softApDistributeIpEvent, WIFI_EVENT_SOFTAPMODE_DISTRIBUTE_STA_IP); 

  ledBlink();

  setNodeId(); /* set the nodeId and the softAP ssid */
  startSoftAP(); /* Start softAP, DNS and HTTP servers for client configuration */
  
   /* 
    * Set a unique mqtt client id (client prefix + node's mac)
    * Set this ID after wifi module (softAP) is configured and up to be able to query its MAC
    * This id is needed to connect to mqtt brokers 
    * If the MAC is not yet available, client IDs from different nodes will be the same (using just the common preffix)
    * and will disconnect each other when trying to connect to a broker!
    */       
    strcpy(mqttClientId, mqttClientPrefix); /* add mqtt client prefix */
    strcat(mqttClientId, nodeId); /* add the full mac (nodeId) */
    
  /*
   * Initialize I2C 
   * 
   * setOutput(&relayState, "1", 1); // turn relay 1 on
   * setOutput(&relayState, "1", 0); // trun relay 1 off
   */
  i2cInit(&relayState); /* Initialize I2C */
  wifiScannedIps = WiFi.scanNetworks(); /* get the number of wifi APs found */


  /////////////////

} /* END OF SETUP */


/***********************************************************/
/***********************************************************/
/***********************************************************/
/* LOOP*/
/***********************************************************/
/***********************************************************/
/***********************************************************/

void loop() {
  monitorEvents(); /* Check if any event flags have been set - wifiStatusChange = true */
  monitorWifi(); /* Connect to a wifi AP if not connected or it is a new config */
  monitorMqtt(); /* service mqtt messages */
} /* END OF LOOP */
