
#define FW_VERSION "1.41"
/*
 *  COMMIT NOTES
 *  
 *  Add delay after WiFi.disconnect(). Without that, ometimes it would fail to reconnect to the AP.
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
 *  ON BOOT, FLASH LED AND ATTEMPT FIRST TO CONNECT TO STORED APs, WITH SOFTAP DISSABLED
 *  IF UNABLE TO CONNECT, ENABLE SOFTAP AND FAST-BLINK THE LED
 *  AFTER ONE MINUTE, IF NO SOFTWAP CLIENTS CONNECTED, DISABLE SOFTAP AND SLOW-BLINK THE LED
 *  
 *  WHEN THE NODE CONNECTS TO MQTT BROKER, FORCE DISCONNECT ALL CLIENTS AND DISABLE SOFTAP
 *  FURTHER CONFIGURATION WILL BE DONE VIA MQTT
 *  
 *  
 */



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
///
/// change DEBUG so it prints selectively DEBUG 0,2 (print only alert and debug)
///
#define DEBUG 2 //Comment this out to disable console messages on production.

#define BAUD 9600 // Debug Serial baud rate.
//#define BAUD 115200 // Debug Serial baud rate.

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
   * https://arduino-esp8266.readthedocs.io/en/latest/libraries.html
   * https://github.com/esp8266/Arduino/blob/master/tools/sdk/include/c_types.h
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


// esp8266 Arduino libraries: https://github.com/esp8266/Arduino/tree/master/libraries
// esp8266 Arduino Core Documentation: https://arduino-esp8266.readthedocs.io/en/latest/
//https://github.com/esp8266/Arduino/blob/master/cores/esp8266/core_esp8266_features.h

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

// appears to be included with some other .h file
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

////////////Change the following two to bool to save space:
//uint8_t mqttBrokerUpA = 0; /* Primary mqtt broker status flag. 0=failed / 1 = OK */
//uint8_t mqttBrokerUpB = 0; /* Backup mqtt broker status flag. 0=failed / 1 = OK */
bool mqttBrokerUpA = false; /* Primary mqtt broker status flag */
bool mqttBrokerUpB = false; /* Backup mqtt broker status flag */
/////////////////////////////////

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


////// PUT ALL THESE FUNCTION DEFINITIONS ON SEPARATE FILES AND SET DEBUG TRAP

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

//bool wifiStationUp = false;
bool wifiStationConnected = false;
bool wifiStationDisconnected = false;
bool wifiReconnectAttempt = false; /* used to ignore wifiStationDisconnected event when attempting to reconnect */

bool wifiStationGotIp = false;
bool wifiSoftApClientGotIp = false;
bool wifiSoftApClientConnected = false;
bool wifiSoftApClientDisconnected = false;
char clientMac[18] = {0};

/* 
 *  TIMERS (Ticker)
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
  

/* Timer to disable softAP - calls softAPoff() - usage: softApTimer.attach(60, softAPoff); */
Ticker softApTimer; 
bool softAPoffFlag = true;
void softAPoff(){
  /* only disable softAP if there are no clients connected */
  if (WiFi.softAPgetStationNum() == 0){                  
    WiFi.softAPdisconnect(true);
    softAPoffFlag = true;
    wifiStatusChange = true;
//    sprint(1, "---------------> called softAPoff()",);
    softApTimer.detach();
  }
}

/* Timer to retry wifi connectivity - calls retryWifi() */
Ticker retryWifiTimer;
bool retryWifiFlag = true;
void retryWifi(){
  /*
   * This function is called by the retryWifiTimer to set the retryWifiFlag
   * When retryWifiFlag is set, the main loop() will disconnect current station-mode wifi
   * and attempt to connect to the given AP.
   * It will also restart the retryWifiTimer.
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
  /* Below print commands are for diagnostics only - Do not leave blocking commands on a callback! */
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
 *  WiFi.onEvent is DEPRECATED!!!
   *  But it is STILL INCLUDED IN THE LATEST ESP8266WiFiGeneric.cpp
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


////// THE ABOVE IS DIFFERENT THAT THIS:
//https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/generic-examples.html
//gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event){

// fROM THE dhcp SIDE esp32
//https://stackoverflow.com/questions/56078329/is-there-a-way-to-get-the-ip-address-that-assigned-to-the-new-sta-that-connected


////TO TEST: GET MAC OF CONNECTED/DISCONNECTED SOFTAP CLIENT
// WiFiEventHandler: https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/generic-class.html#wifieventhandler
// Code Example: https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/generic-examples.html#the-code
//
// Note: The above client connect and disconnet events do not have the IP of the device, just the MAC:
// https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/ESP8266WiFiType.h
//
//    struct WiFiEventSoftAPModeStationConnected{
//        uint8 mac[6];
//        uint8 aid;
//    };
//    struct WiFiEventSoftAPModeStationDisconnected{
//        uint8 mac[6];
//        uint8 aid;
//    };
/*
 * WiFiEventSoftAPModeStationConnected sta_info
 * WiFiEventSoftAPModeStationDisconnected sta_info
 * 
 * https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/generic-class.html
 * WiFiEventHandler  onSoftAPModeStationConnected (std::function< void(const WiFiEventSoftAPModeStationConnected &)>)
 * WiFiEventHandler  onSoftAPModeStationDisconnected (std::function< void(const WiFiEventSoftAPModeStationDisconnected &)>)
 * 
 * https://github.com/esp8266/Arduino/issues/3638
 * https://github.com/esp8266/Arduino/issues/2100:
 */



// FOR TEST ONLY - Enable CALLBACK REGISTRATION IN SETUP()
// https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/WiFiEvents/WiFiEvents.ino
WiFiEventHandler probeRequestPrintHandler;
void onProbeRequestPrint(const WiFiEventSoftAPModeProbeRequestReceived& evt) {
  Serial.print("Probe request from: ");
  //Serial.print(macToString(evt.mac));
  char macStr[18];
  // Copy the sender mac address to a string
  snprintf(macStr, sizeof(macStr), MACSTR, MAC2STR(evt.mac));
  Serial.print(macStr);
  Serial.print(" RSSI: ");
  Serial.println(evt.rssi);
}


///////////////////////////////
///DID NOT TEST THIS ONE..... this might be for the ESP32.. !!!
////////////////////
///TEST -- it probably wont work because there is no event for distributed IP defined on WiFiEventHandler
//WiFiEventHandler softApClientIp;
////https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/ESP8266WiFiGeneric.cpp
//https://bbs.espressif.com/viewtopic.php?t=1927
//
// CAN WE USE THIS WITH:  WIFI_EVENT_SOFTAPMODE_DISTRIBUTE_STA_IP ????????????????????
//
//void onSoftApClientIp(System_Event_t *event) {
//  switch (event->event) {
//    case WIFI_EVENT_SOFTAPMODE_STACONNECTED:
//    case WIFI_EVENT_SOFTAPMODE_STADISCONNECTED: {
//      char mac[32] = {0};
//      snprintf(mac, 32, MACSTR ", aid: %d" , MAC2STR(event->event_info.sta_connected.mac), event->event_info.sta_connected.aid);
//      Serial.println(mac);
//    }
//    break;
//  }
//}
//


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

/*
 * showFreeHeapOnChange()
 * 
 * Monitors memory leaks
 * Display a message when the free heap varies more than 1000
 * https://github.com/esp8266/Arduino/blob/master/cores/esp8266/Esp.cpp
 * uint32_t EspClass::getFreeHeap(void)
 */
uint32 prevFreeHeap = 0; 
void showFreeHeapOnChange(){  
  if ( prevFreeHeap > ESP.getFreeHeap()+5000 ){
    sprint(0, "FREE HEAP DECREASED TO: ", ESP.getFreeHeap()); 
  }else{  
    if ( prevFreeHeap + 5000 < ESP.getFreeHeap()){
      sprint(2, "FREE HEAP INCREASED TO: ", ESP.getFreeHeap());
    }
  }
  if (ESP.getFreeHeap() < 20000){
    sprint(0, "FREE HEAP IS GETTING LOW. POTENTIAL MEMORY LEAK! - ", ESP.getFreeHeap());
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
    if(softAPoffFlag){
      sprint(2, "SOFTAP POINT DISSABLED", );
    }else{
      sprint(2, "SOFTAP POINT ENABLED. Connected Clients: ", WiFi.softAPgetStationNum());
    }
    sprint(2,"Heap Fragmentation (< 50% is good): ", ESP.getHeapFragmentation());  /* 0% is clean. 50% OK */
    if (ESP.getHeapFragmentation() > 50){
      sprint(0, "HEAP FRAGMENTATION IS HIGH: ", ESP.getHeapFragmentation());
    }
    showFreeHeapOnChange();
    sprint(2,"",); /* add a blank line for readability */
  }
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
//  sprint(1, "LOOP PASS",);

  monitorEvents(); /* Check if any event flags have been set if wifiStatusChange */
 
  if(retryWifiFlag){
    retryWifiFlag = false; /* don't check again until after new retry time is up*/
    /*
     * getNextAp()
     * 
     * selects the next AP:
     * get settings from flash (not RAM)
     * 0: use current apSSIDlast
     * 1: use A
     * 2: use B
     * 3: use factory 
     * Increase count and reset to 0 if 3 or more
     * If contents of selected ssid are null, get next one
     * on connect, set count to zero
     */

     //////    /// PUT THIS IN A SEPARATE FUNCTION! 
         
    sprint(2, "CONNECTING TO WIFI SSID: ", cfgSettings.apSSIDlast);
    
    wifiReconnectAttempt = true; /* Ignore the known disconnect event that we are causing */  
    // test
    sprint(1, "-----------------> WIFI MODE before: ", WiFi.getMode());  
//    WiFi.disconnect(); /* Clear previous connection - will generate a disconnect event */
    WiFi.disconnect(true); /* Clear previous connection - will generate a disconnect event */
/////////////////////////////
    delay(500); /* without some delay between disconnect and begin, it fails occasionally to connect */
    WiFi.mode(WIFI_AP_STA); /* set wifi for both client (station) and softAP - It was working without this, so it might be the default */
    WiFi.setAutoConnect(false); /* prevents the station client trying to reconnect to last AP on power on/boot */
    WiFi.setAutoReconnect(false); /* prevents reconnecting to last AP if conection is lost */
///////////////////////////////
    WiFi.begin(cfgSettings.apSSIDlast, cfgSettings.apPSWDlast); /* connect to the last ssid/pswd in ram */

    /* start wifi retry timer - calls retryWifi() on timeout*/
    if (WiFi.softAPgetStationNum() > 0){ /* Clients connected */
      retryWifiTimer.attach(30, retryWifi); /* wait longer to retry when a client is connected to prioritize configuration tasks over wifi reconnects */
    }else{ /* no clients connected. Retry more often */
      retryWifiTimer.attach(10, retryWifi); 
    }
  }
  /* service dns requests from softAP clients */
  if (!softAPoffFlag){ //////////////// CONVERT THIS TO SOFTAPUP FLAG
      dnsServer.processNextRequest();
      //////
      //If the ESP8266‘s station interface has been scanning or trying to connect to a target router, the ESP8266 softAP-end connection may break.
      //if not connected to an AP, set mode to softAP ONLY, so the channel does not change while user is configuring wifi.
      httpServer.handleClient(); /* service http requests ONLY from softAP */   
  }
//  httpServer.handleClient(); /* service http requests from softAP AND wlan clients */

  if (internetUp){
    manageMqtt();
  }else{
    resetMqttBrokerStates();
    checkInternet();
  }
  
  showFreeHeapOnChange(); /* monitor memory leaks */
    
} /* END OF LOOP */
