
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

//////////////////////////////////////////////////////////////
  // MOVE THIS TO THE SETUP SECTION
  /*
   * softAP + Station mode FAQ: https://bbs.espressif.com/viewtopic.php?f=10&t=324 
   * in softAP + station mode, ESP8266 softAP will adjust its channel configuration to be as same as ESP8266 station.
   * WiFi.mode(m): set mode to WIFI_AP, WIFI_STA, WIFI_AP_STA or WIFI_OFF
   * https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/generic-class.html#mode
   * https://stackoverflow.com/questions/59096373/differents-between-wifi-mode-and-wifi-set-opmode
   * 0x01: Station mode
   * 0x02: SoftAP mode
   * 0x03: Station + SoftAP
   */
  WiFi.mode(WIFI_AP_STA); /* set wifi for both client (station) and softAP - The default mode is SoftAP mode */
  // THIS IS ONLY FOR STATION MODE CLIENT ACCESS POINT!! ///////////////
  /* https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/station-class.html#setautoreconnect */
  WiFi.setAutoConnect(false); /* prevents the station client trying to reconnect to last AP on power on/boot */
  WiFi.setAutoReconnect(false); /* prevents reconnecting to last AP if conection is lost */
    //////////////////////////////////////////////

//The maximum number of stations that can simultaneously be connected to the soft-AP can be set from 0 to 8, but defaults to 4.
//defaults: WiFi.softAP(ssid, password, channel=1, hidden=false, max_connection=4)
//WiFi.softAP(softAP_ssid, softAP_password, 6, false, 4);
  if (WiFi.softAP(softAP_ssid, softAP_password) ){
    sprint(1, "SOFTAP ENABLED. SSID: ", softAP_ssid);
    sprint(2, "SoftAP IP Address: ", WiFi.softAPIP());
    startDnsServer();
    startHttpServer();
  }else{
    sprint(0, "SOFTAP INITIALIZATION FAILED!",);
  }
 }


void monitorWifiAP(){
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
    WiFi.disconnect(true); /* Clear previous connection and stop station mode - will generate a disconnect event */
    delay(500); /* without some delay between disconnect and begin, it fails occasionally to connect to an AP */
    WiFi.mode(WIFI_AP_STA); /* set wifi for both client (station) and softAP - It was working without this, so it might be the default */
    WiFi.setAutoConnect(false); /* prevents the station client trying to reconnect to last AP on power on/boot */
    WiFi.setAutoReconnect(false); /* prevents reconnecting to last AP if conection is lost */
///////////////////////////////
    WiFi.begin(cfgSettings.apSSIDlast, cfgSettings.apPSWDlast); /* connect to the last ssid/pswd in ram */
//    connectWifi();
    /* start wifi retry timer - calls retryWifi() on timeout*/
    if (WiFi.softAPgetStationNum() > 0){ /* Clients connected */
      retryWifiTimer.attach(30, retryWifi); /* wait longer to retry when a client is connected to prioritize configuration tasks over wifi reconnects */
    }else{ /* no clients connected. Retry more often */
      retryWifiTimer.attach(10, retryWifi); 
    }
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

void monitorDns(){
  /* service dns requests from softAP clients */
  if (!softAPoffFlag){ //////////////// CONVERT THIS TO SOFTAPUP FLAG
      dnsServer.processNextRequest();
      //////
      //If the ESP8266‘s station interface has been scanning or trying to connect to a target router, the ESP8266 softAP-end connection may break.
      //if not connected to an AP, set mode to softAP ONLY, so the channel does not change while user is configuring wifi.
      httpServer.handleClient(); /* service http requests ONLY from softAP */   
  } 
  /// ELSE: DISABLE DNS SERVER!
  ///////////////////////////////// 
////  httpServer.handleClient(); /* service http requests from softAP AND wlan clients */
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
  ////////////
  //// CHECK HERE THAT THE WEB SERVER DID NOT RETURN AN ERROR CODE
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
   * Get the public IP address (if connected to wifi)
   * If it succeeds, set the internetUp and wifiStatusChange flags
   */   
   if(!WiFi.isConnected()){
    internetUp = false;
    return;    
   }
   /*
    * https://www.reddit.com/r/esp8266/comments/ewt483/confused_about_deprecated_httpclientbegin_calls
    * https://community.platformio.org/t/bool-httpclient-begin-string-is-deprecated/16924/2 
    * https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/client-class.html
    */
   WiFiClient client; ////// do we need to close this one? change to HTTPClient httpClient;
   //// TESTING: TO DECREASE RESPONSE TIME
   client.setNoDelay(true); /* disable Nagle algorithm */

   HTTPClient http;
    //////////////////////////////////////////////////////////
    /////// >>> MAKE EACH SERVICE SITE CONFIGURABLE VIA MQTT
    ///////// TEST TWO OR MORE SEPARATE SITES
    ///////// REPORT QUESTIONABLE INTERNET ACCESS IF ONE OF THE SITES FAILED
    ////////// BUT IF MQTT BROKER IS RESPONDING DO NOT RETRY WIFI ACCESS POINT
    //////////////////////////////////////////////////////////
   
   /*
    * https://github.com/esp8266/Arduino/pull/4038
    * https://github.com/espressif/arduino-esp32/issues/1433#issuecomment-473460315
    * Setting a time of less than 5 seconds causes the http.GET() to exit in just 50 ms bypassing the timeout most of the time! Why?
    * If if returns with a code > 0 it skips the timeout.
    * If the code is < 0 and the timeout is > 6sec it will wait the time before http.GET() returns
    */
  http.setTimeout(3000);// 20K= 32s, 10K=20s, 5K=10s, 3K= 6s, 2K5= 5s (minimum time), 2K= 4s(sometimes) 1K= too small!
  ///////////////////////////////////////////////////////////
  
  //http.begin("http://api.ipify.org/?format=text");
  http.begin(client, "http://api.ipify.org/?format=text");
  // for testing:
  int prevTime = millis(); 
  sprint(1, "test", );
  ///////////////////////////
  /*
   * https://techtutorialsx.com/2016/07/17/esp8266-http-get-requests/
   * https://techtutorialsx.com/2016/07/21/esp8266-post-requests/
   */
  int httpCode = http.GET(); /* Send http GET request to API */
  sprint(1, "GET time: ", millis()- prevTime);
  sprint(1,"HTTP RETURN CODE: ", httpCode);
  /*
   * if the value is greater than 0, it corresponds to a standard HTTP code. 
   * If this value is less than 0, it corresponds to a ESP8266 error, related with the connection.
   * https://github.com/esp8266/Arduino/issues/5137#issue-360559415
   */
  if (httpCode == 200){ /* Got a valid response - Internet is up */
    /*
     * Return codes:
     * https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPClient/src/ESP8266HTTPClient.h#L45
     * 
     * convert const String (http.getString)to const char* (wanIp)
     * option 1: http.getString().toCharArray(wanIp, http.getString().length()+1);
     * option 2: sprintf(wanIp, "%s", http.getString().c_str());
     */
    ///// change sprintf to the more secure snprintf (will need to add the sizeof the string plus one for the null terminator
    sprintf(wanIp, "%s", http.getString().c_str());
    internetUp = true;
    wifiStatusChange = true;
  }else{
    /* WiFI is up but either with no internet access, or test site is not sending a valid response */
    //check here:
    // if <0, Local Connectivity problem
    // if >0, http error
    ///////////////////////////////////
    Serial.print("{*}");
//    sprint(0, "WiFI is Up but no Internet",); 
    internetUp = false;
  }
  http.end();   //Close connection
  client.stop();
 }



//NEEDS TO BE CHANGED: IT ONLY CONNECTS TO ONE AP
//NEEDS TO CHECK MULTIPLE APs, WITH THE SAME or different SSID and/or passwords
// AND fallback to FACTORY TEST SSID 
// make the factory ssid match the node mac as password? That way it is unique!

// Load values from flash into struct, or null for none
//build functions to:
//getPreviousAP() //retrieve wifi APs in LIFO order
//saveAP() //store current wifi AP in LIFO order

// modify the eeprom fuctions 'saveAll' and 'getSettings' to accept the start address in flash as argument
// currently assuming start address 0
