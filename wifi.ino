
void startSoftAP(){
  /*
   * Starts the wifi soft access point used for initial wifi client configuration
   * Starts the http server to handle requests via the soft AP and WLAN 
   * Starts the dns server to redirect all requested domains to the apIP IP (10.1.1.1)
   * https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/soft-access-point-class.html
   * https://lastminuteengineers.com/creating-esp8266-web-server-arduino-ide/
   * 
   * soft-AP and station mode share the same hardware channel: the soft-AP channel will default to the number used by station. 
   * See FAQ: https://bbs.espressif.com/viewtopic.php?f=10&t=324
   */  
  sprint(2, "Configuring Soft Access Point...", );
  /* 
   *  set SSID to "SFM_Module" + last 4 digits of wifi client's MAC address  
   * 
   * Convert const String (WiFi.macAddress)to const char* (nodeId)
   * NOTE: For SN identification purposes use only the station mac - WiFi.macAddress() - it is registered to espressif. 
   * The softAP mac - WiFi.softAPmacAddress() - is unregistered.
   * // sprint(2, "----> STATION MAC: ", WiFi.macAddress().c_str()); ----> STATION MAC: F4:CF:A2:71:B0:33 (espressif)
   * // sprint(2, "----> SOFTAP MAC: ", WiFi.softAPmacAddress().c_str()); ----> SOFTAP MAC: F6:CF:A2:71:B0:33 (not registered)
   */
  WiFi.macAddress().toCharArray(nodeId, WiFi.macAddress().length()+1);
  /* remove mac address character ":" from nodeId */
  removeChar(nodeId, ':'); //nodeId without ':' has now 12 characters + \0 = 13 bytes
  /* get the last 4 digits of the 12 digits of nodeId (plus the string terminator \0 = 5 characters) to make a shorter SSID */
  strcat(softAP_ssid, APSSID);
  strcat(softAP_ssid, &nodeId[8]); //copy from position 8 until the end of the string \0
  sprint(2, "Node's MAC Address: ", nodeId);
  /* Remove the password parameter (or set it to null to join the AP with only the ssid */
  WiFi.softAPConfig(apIP, apIP, netMsk);
  /* softAP + Station mode FAQ: https://bbs.espressif.com/viewtopic.php?f=10&t=324 */
  WiFi.mode(WIFI_AP_STA); /* set wifi for both client (station) and softAP - It was working without this, so it might be the default */
  /* https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/station-class.html#setautoreconnect */
  WiFi.setAutoConnect(false); /* prevents the station client trying to reconnect to last AP on power on/boot */
  WiFi.setAutoReconnect(false); /* prevents reconnecting to last AP if conection is lost */
  if (WiFi.softAP(softAP_ssid, softAP_password) ){
    sprint(1, "SOFTAP ENABLED. SSID: ", softAP_ssid);
    sprint(2, "SoftAP IP Address: ", WiFi.softAPIP());
    startDnsServer();
    startHttpServer();
  }else{
    sprint(0, "SOFTAP INITIALIZATION FAILED!",);
  }
 }
 
void startDnsServer(){
  /*
   * Start dns server
   * if DNSServer is started with with domain name = "*", 
   * it should resolve all domains like zzzz.com with the IP apIP. But it takes a long time before it does.
   * It seems to be a problem with the client browsers because the node does not print any debug until it finally serves the page.
   * 
   * https://github.com/esp8266/Arduino/blob/master/libraries/DNSServer/examples/DNSServer/DNSServer.ino
   */
  //dnsServer.setTTL(300); /* Default is 60 seconds.  https://www.varonis.com/blog/dns-ttl/ */
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);
}

void startHttpServer(){
  /*
   * The httpServer responds to http requests from clients connected to the softAP and WLAN
   * 
   * Sets up handler functions for: 
   * * wifi config (on root /)
   * * wifisave
   * * not found.
   */
  httpServer.on("/", handleWifi);
  httpServer.on("/wifisave", handleWifiSave);
  httpServer.on("/configWait", handleConfigWait);
  httpServer.onNotFound(handleWifi);
  httpServer.begin(); // Web server start
  //// CHECK HERE THAT THE WEB SERVER DID START WITHOUT ERROR CODE
  sprint(2, "HTTP server started",);
  softApTimer.attach(60, softAPoff); /* Disable softAP after 60 seconds */
  softAPoffFlag = false;
}


void showSoftApClients(){
  /*
   * Disply the devices connected with their MAC and assigned IP
   * 
   * Right after a client reports as connected (via an event), it takes a few more seconds to get an IP assigned
   * Wait for the callback that sets up the flag wifiSoftApClientGotIp
   * 
     * https://techtutorialsx.com/2019/08/11/esp32-arduino-getting-started-with-wifi-events/
     * https://www.esp8266.com/viewtopic.php?f=32&t=5669&start=4
     * https://learn.adafruit.com/mac-address-finder/the-code
     * https://www.espressif.com/sites/default/files/documentation/2c-esp8266_non_os_sdk_api_reference_en.pdf
   * 
   * WiFi.softAPgetStationNum() reports current number of clients connected
   * wifi_softap_get_station_info() reports the MAC and IP of connected clients AFTER DHCP has assigned IPs
   * So, the number of clients connected might be temporarily more than the clients reported with MAC/IPs.
     * https://bbs.espressif.com/viewtopic.php?t=1927
   * 
   * The MAC not accesible by wifi_softap_get_station_info() right after a connect/disconnect onEvent triggers
     * Need to wait for DHCP to assign an IP.
     * https://github.com/esp8266/Arduino/issues/2100
   * 
   * However, the MAC is available immediately with the events WiFi.onSoftAPModeStationConnected/Disconnected
   *
   * See SDK files for details of struct station_list and linked list control STAILQ_NEXT
   * 
   * https://github.com/mattcallow/esp8266-sdk/blob/master/esp_iot_sdk/include/user_interface.h
   * https://github.com/scottjgibson/esp8266/blob/master/esp_iot_sdk_v0.6/include/queue.h
   * 
   * #define   MACSTR   "%02x:%02x:%02x:%02x:%02x:%02x"
   * #define   MAC2STR(a)   (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
   * 
   *   struct station_info {    
   *    STAILQ_ENTRY(station_info)  next;
   *    
   *    uint8 bssid[6];
   *    struct ip_addr ip;
   *   };
   *   
   *   STAILQ_NEXT is defined in queue.h 
   *   #define STAILQ_NEXT(elm, field) ((elm)->field.stqe_next)
   *   
   *   SDK function wifi_softap_get_station_num() is the same as WiFi.softAPgetStationNum()
   */
  sprint(2, "CONNECTED SOFTAP CLIENTS: ", wifi_softap_get_station_num() );  
  struct station_info *station_list = wifi_softap_get_station_info();
  while (station_list != NULL) {
    /* get MAC */
    char station_mac[18] = {0}; 
    snprintf(station_mac, sizeof(station_mac), MACSTR, MAC2STR(station_list->bssid));
    /* get IP */
    String station_ip = " - " + IPAddress((&station_list->ip)->addr).toString();
    sprint(2, station_mac, station_ip);
    station_list = STAILQ_NEXT(station_list, next);
  }
  wifi_softap_free_station_info(); /* free memory */
}



// void connectWlan() {
//  /*
//   * Attempt to connect to the provided ssid
//   * Resets the timer to start another wifi reconnect cycle
//   */
//  sprint(1, "---------------------------------------",);
//  sprint(1, "internetUp Flag: ", internetUp); 
//  sprint(1, "---------------------------------------",);
//  prevWifiStationState = WL_IDLE_STATUS; /* reset current wifi state flag to force checking internet access by fetching the public IP*/  
//  resetMqttBrokerStates(); /* Since we are re/connecting to wifi, set mqtt brokers status offline */ 
//  sprint(2, "Connecting to WiFi AP: ", cfgSettings.apSSIDlast);
//  }
// } 


 void loadWifiDefaults(){
  /*
   * reset wifi AP to factory default
   */
  strcpy(cfgSettings.apSSIDlast, apSSIDfact);
  strcpy(cfgSettings.apPSWDlast, apPSWDfact);
  sprint(2,"LOADED DEFAULT WIFI AP",);
 }


 void checkInternet(){
  /*
   * Get the public IP address if connected to wifi
   * If it succeeds, set the internetUp flag
   */   
   if(!WiFi.isConnected()){
    internetUp = false;
    return;    
   }
  HTTPClient http;
  //////////////////////////////////////////////////////////
  /////// >>> MAKE EACH SERVICE SITE CONFIGURABLE VIA MQTT
  ///////// TEST TWO OR MORE SEPARATE SITES
  ///////// REPORT QUESTIONABLE INTERNET ACCESS IF ONE OF THE SITES FAILED
  ////////// BUT IF MQTT BROKER IS RESPONDING DO NOT RETRY WIFI ACCESS POINT
  //////////////////////////////////////////////////////////
  http.begin("http://api.ipify.org/?format=text");
  int httpCode = http.GET(); /* Send http GET request to API */
  /*
   * if http.get return code is less than zero, asume internet down
   * https://github.com/esp8266/Arduino/issues/5137#issue-360559415
   */
  if (httpCode > 0){ /* internet is up */
    /* convert const String (http.getString)to const char* (wanIp) */
    //////////////////////////////////////////////////////////////////////////////////////
    //http.getString().toCharArray(wanIp, http.getString().length()+1);
    //another way:
    ///// change sprintf to the more secure snprintf (will need to add the sizeof the string plus one for the null terminator
    sprintf(wanIp, "%s", http.getString().c_str());
//    sprint(2,"EVENT: INTERNET IS UP - Public IP: ", wanIp);
    internetUp = true;
    wifiStatusChange = true;
  }else{
    /* No internet access - reset flags */
//    sprint(0, "WiFI is Up but no Internet",); 
    internetUp = false;
  }
  http.end();   //Close connection
 }



//NEEDS TO BE CHANGED: IT ONLY CONNECTS TO ONE AP
//NEEDS TO CHECK MULTIPLE APs, WITH THE SAME or different SSID and/or passwords
// AND fallback to FACTORY TEST SSID 
//Do we want to make the factory ssid match the node mac as password? That way it is unique!

// Load values from flash into struct, or null for none
//build functions to:
//getPreviousAP() //retrieve wifi APs in LIFO order
//saveAP() //store current wifi AP in LIFO order

// modify the eeprom fuctions 'saveAll' and 'getSettings' to accept the start address in flash as argument
// currently assuming start address 0
