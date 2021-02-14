

void setNodeId(){
  /* 
   *  Sets nodeId to the mac address of the esp8266 and 
   *  Set SSID (used by the softAP) to "SFM_Module" + last 4 digits of the mac address  
   * 
   * Convert const String (WiFi.macAddress)to const char* (nodeId)
   * NOTE: For SN identification purposes use only the station mac - WiFi.macAddress()since it is registered to espressif. 
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
}

// THIS FUNCTION NEEDS TO BE FIXED
// 1) SOMETIMES TAKES TOO MANY RETRIES TO CONNECT TO SFM1 OR TEST1
// IT IS POSSIBLE THAT THE ABOVE IS A PROBLEM WITH THE ROUTER. SEE LOGS
// 2) ENFORCE NO SOFTAP ON BOOT AND IF NOT FIRST PASS.

void monitorWifi(){
  monitorSoftApClients();  /* service dns and http requests if softAP is enabled */  
  if(retryWifiFlag){ /* Connect to wifi AP */
    retryWifiFlag = false; /* don't check again until after new retry time is up*/
    /*
     * TODO:
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
    sprint(2, "CONNECTING TO WIFI SSID: ", cfgSettings.apSSIDlast);
    wifiReconnectAttempt = true; /* Ignore the known disconnect event that we are causing */  
    WiFi.disconnect(true); /* Clear previous connection and stop station mode - will generate a disconnect event */
   
    ////////////////////////////////

    //we might need to set both station and softap or it will disconnect the client if an incorrect ssid/pswd is given
    /////////////////////////////////////
    if(softApUp){
      WiFi.mode(WIFI_AP_STA); /* set wifi for both client (station) and softAP */      
    }else{
      WiFi.mode(WIFI_STA); /* set wifi for station mode only so it does not broadcast ssid */
    }
    wifiStationRetries++;
    sprint(1, "WIFI Station Retries: ", wifiStationRetries);
    ///////////////////////////////////////        
    delay(500); /* without some delay between disconnect and begin, it occasionally fails to connect to the AP */
    WiFi.setAutoConnect(false); /* prevents the station client from trying to reconnect to last AP on power on/boot */
    WiFi.setAutoReconnect(false); /* prevents reconnecting to last AP if connection is lost */
///////////////////////////////
    WiFi.begin(cfgSettings.apSSIDlast, cfgSettings.apPSWDlast); /* connect to the last ssid/pswd in ram */
    
    /////////////////
    //// set config softap just in case... FIND A BETTER WAY. DO NOT SET IF CONFIG IF SOFTAP IS NOT ENABLED!
    delay(200); //THIS DELAY MINGT NOT BE NEEDED
//    WiFi.softAPConfig(apIP, apIP, netMsk);
    
    /////////////////////////////////////////////
    /* start wifi retry timer - calls retryWifi() on timeout*/
    retryWifiTimer.attach(10, retryWifi); /* retry wifi again in 5 seconds if not connected to mqtt broker */
    
//    if (WiFi.softAPgetStationNum() > 0){ /* Clients connected */
//      retryWifiTimer.attach(30, retryWifi); /* wait longer to retry when a client is connected to prioritize configuration tasks over wifi reconnects */
//    }else{ /* no clients connected. Retry more often */
//      retryWifiTimer.attach(10, retryWifi); 
//    }
/////////////////////////////////////////////////
  }
}

void startSoftAP(){
  /*
   * Starts the wifi soft access point used for initial wifi client configuration on boot
   * Starts the http server to handle requests via the soft AP and WLAN 
   * Starts the dns server to redirect all requested domains to the http server IP (apIP)
   * References
     * https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/soft-access-point-class.html
     * https://lastminuteengineers.com/creating-esp8266-web-server-arduino-ide/
     */  
  sprint(2, "Starting Soft Access Point...", );
/////////////// TESTING. This does not seem to make a difference:
  WiFi.disconnect(true); //Disable STA
  WiFi.mode(WIFI_OFF);
  ////////////////////////
  delay(200);  
  /*
   * soft-AP and station modes share the same (single) hardware channel 
   * the soft-AP channel will be set to the channel assigned to the station. 
   * softAP clients connected with the default channel, before the station mode is up, 
   *  may lose the connection if assigned channel changes.
   * References:
     * https://bbs.espressif.com/viewtopic.php?f=10&t=324
     * WiFi.mode(m): set mode to WIFI_AP, WIFI_STA, WIFI_AP_STA or WIFI_OFF
     * https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/generic-class.html#mode
     * https://stackoverflow.com/questions/59096373/differents-between-wifi-mode-and-wifi-set-opmode
     * 0x01: Station mode
     * 0x02: SoftAP mode
     * 0x03: Station + SoftAP
   */
   /////////////////////////////////
  WiFi.mode(WIFI_AP_STA); /* set wifi for both client (station) and softAP - The default mode is SoftAP mode */
//  WiFi.mode(WIFI_AP); /* set wifi for only softAP */
  /*
   * The maximum number of stations that can simultaneously be connected to the soft-AP can be set from 0 to 8. 
   * The default is 4.
   * defaults: WiFi.softAP(ssid, password, channel=1, hidden=false, max_connection=4)
   */
  //WiFi.softAP(softAP_ssid, softAP_password, 6, false, 4);
  if (WiFi.softAP(softAP_ssid, softAP_password) ){ /* start softAP */
    ////////////////////////////
    delay(200); /* wait until event SYSTEM_EVENT_AP_START has fired, before setting configuration. 
    /* https://github.com/espressif/arduino-esp32/issues/985#issuecomment-359157428 */
    /* Remove the password parameter (or set it to null to join the AP with only the ssid */
    WiFi.softAPConfig(apIP, apIP, netMsk);

   ///////////////////////////
    sprint(1, "SOFTAP ENABLED. SSID: ", softAP_ssid);
    sprint(2, "SoftAP IP Address: ", WiFi.softAPIP());
    startDnsServer();
    startHttpServer();
    softApUp = true; /* set the access point up flag only after both the dns and http servers are up */
    softApTimer.attach(60, checkSoftAp); /* Disable softAP after 60 seconds if no clients are attached */  
  }else{
    sprint(0, "SOFTAP INITIALIZATION FAILED!",);
  }
}



void monitorSoftApClients(){
  /*
   * service dns and http requests from softAP connected clients 
   */
  if (softApUp){
      dnsServer.processNextRequest();
      httpServer.handleClient(); /* service http requests ONLY from softAP */ 
  } 
}


void startDnsServer(){
  /*
   * if DNSServer is started with domain name = "*", 
   * it will resolve all domains like zzzz.com with the IP apIP. 
   * https://github.com/esp8266/Arduino/tree/master/libraries/DNSServer
   * https://github.com/esp8266/Arduino/blob/master/libraries/DNSServer/examples/DNSServer/DNSServer.ino
   * https://github.com/esp8266/Arduino/blob/master/libraries/DNSServer/src/DNSServer.h#L58
   */
  dnsServer.setTTL(60); /* Default is 60 seconds.  https://www.varonis.com/blog/dns-ttl/ */
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP); // Returns true if successful, false if there are no sockets available
  sprint(2, "DNS Server Started", );
}


void startHttpServer(){
  /*
   * The httpServer responds to http requests from clients connected to the softAP and WLAN
   * https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer
   * 
   * Sets up handler functions for: 
   * * / 
   * * /wifisave
   * * /configWait
   * * Not Found.
   */
  httpServer.on("/", handleWifi);
  httpServer.on("/wifisave", handleWifiSave);
  httpServer.on("/configWait", handleConfigWait);
  httpServer.onNotFound(handleWifi);
  httpServer.begin(); // Start Web Server
  sprint(2, "HTTP server started",);
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
   * If it succeeds, set the internetUp and nodeStatusChange flags
   */   
   if(!WiFi.isConnected()){
    internetUp = false;
    strcpy(wanIp, "0.0.0.0"); /* Clear WAN IP */
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
  /////////////////////////////////////////////////////////
  // for testing:
  int prevTime = millis(); 
  sprint(1, "test", );
  /////////////////////////////////////////////////////////
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
    nodeStatusChange = true;
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
