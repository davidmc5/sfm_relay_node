void startAP(){
  /*
   * Starts the soft access point 
   * and sets the http server to handle requests via the soft AP as well as the WLAN 
   */  
  sprint(2, "Configuring Access Point...", );
  
  /* set SSID to "SFM_Module" + last 4 digits of wifi client's MAC address */
  /* https://github.com/esp8266/Arduino/blob/4897e0006b5b0123a2fa31f67b14a3fff65ce561/libraries/ESP8266WiFi/src/ESP8266WiFiSTA.cpp#L368 */
  /* https://forum.arduino.cc/index.php?topic=495247.msg3378596#msg3378596 */
  /* convert const String (WiFi.macAddress)to const char* (nodeId) */
  WiFi.macAddress().toCharArray(nodeId, WiFi.macAddress().length()+1);

  /* remove mac address character ":" from nodeId */
  removeChar(nodeId, ':'); //nodeId without ':' has now 12 characters + \0 = 13 bytes
  
  //get the last 4 digits of the 12 digits of nodeId (plus the string terminator \0 = 5 characters) to make a shorter SSID
  strcat(softAP_ssid, APSSID);
  strcat(softAP_ssid, &nodeId[8]); //copy from position 8 until the end of the string \0
  
  sprint(2, "MAC Address", nodeId);
  sprint(2, "SoftAP SSID", softAP_ssid);
  sprint(2, "SoftAP Password", softAP_password);

  /* Remove the password parameter (or set it to null to join the AP with only the ssid */
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(softAP_ssid, softAP_password);

  ////not sure if we need this
  delay(200); // Without delay it might leave the IP address blank
  sprint(2, "SoftAP IP Address", WiFi.softAPIP()); 
  
  /* Setup the DNS server redirecting all the domains to the apIP */
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  
  /*
   * if DNSServer is started with "*" for domain name, it will reply with
   * the provided apIP to all DNS request
   */
  dnsServer.start(DNS_PORT, "*", apIP);
//////////
//////////
//////////
//// THIS HTTP SERVER IS NOT *ONLY* PART OF THE SOFT AP BUT ALSO SERVES THE WLAN. 
  /*
   * The httpServer responds to http requests from clients connected to the softAP
   * as well as clients on the WLAN interface
   * 
   * Setup web pages: 
   * *root
   * *wifi config
   * *captive portal detectors
   * *not found.
   */
//  server.on("/", handleRoot);
  httpServer.on("/", handleWifi);
  httpServer.on("/wifi", handleWifi);
  httpServer.on("/wifisave", handleWifiSave);
  httpServer.onNotFound(handleNotFound);
  
  httpServer.begin(); // Web server start
  sprint(2, "HTTP server started",);
  
//  /* Request to Connect (TRUE) to Access Point if there is a valid SSID (non-zero length) */
//  connect = strlen(cfgSettings.ap_ssid) > 0; 
 }



/*
 * manageWifi()
 * 
 * Checks if connection is up to wifi AP
 * if not, attempt to connect
 * Also connect if the 'connect' flag has been set
 * connect = true forces a test/reconnect to the current ssid/pswd(s)on ram
 */

/* manage connection to AP */
 void manageWifi(){
  /*
   * Do not set wifiUp to true here, even if connected to wifi, 
   * since it might have been set to false by the captive portal to test new given AP in ram
   * The function connectWlan sets that flag to true if connected to wifi
   */
  if (WiFi.status() != WL_CONNECTED){ /* set the connect flag if not connected to wifi */
    wifiUp = false;
  }

  
  if (!wifiUp || !internetUp) { 
    /* Disconnect from current AP and attempt to connect to the current ssid on ram */  
    connectWlan();  /* attempt to connect to the given access point */  
  }
  currentWifiState = WiFi.status();
    /* 
     *  If not connected to WLAN and timeout exceeded, assume RAM settings are invalid
     *  Restore settings from flash
     */
  if (currentWifiState != WL_CONNECTED && millis() > (lastConnectTry + 15000)) {
    /* Don't set retry time too low as it will interfere with the softAP operation */
    sprint(0, "RESETTING WIFI - RESTORING SETTINGS FROM FLASH",);
    getCfgSettings(); /* recover settings from flash */
    wifiUp = false; /* set the reconnect flag back to connect to the recovered flash settings */
  }
  if (lastWifiState != currentWifiState) { /* status changed since last loop */
    sprint(2, "WiFi Status Changed From", wifiStates[lastWifiState]);
    sprint(2, "WiFi Status Changed To", wifiStates[currentWifiState]);    
    lastWifiState = currentWifiState;
    if (currentWifiState == WL_CONNECTED) {
      /* Just connected to WLAN */
      sprint(2, "WiFI Client Connected To", cfgSettings.ap_ssid);
      sprint(2, "WiFi Client IP Address", WiFi.localIP());

        //////////////////////////////////////////////////////
        /// PLACING THIS BLOCK INSIDE THE CONNECTED BLOCK...
        /// MDNS DOES NOT SEEM TO WORK SOMETIMES... ADDING A SMALL DELAY DOES NOT WORK EITHER...
        // THIS ONLY WORKS THE ON RESET, ON INITIAL BOOT
        // BUT IF WE GET HERE AFTER CHANGING THE AP VIA THE CAPTIVE PORTAL, THE MDNS.BEGUIN RETURNS FALSE!
        //
        /// and not sure now what mdns does...!?
        delay(300); //without delay does not work. Trying with some delay... DOES NOT WORK EITHER
        // Setup MDNS responder
        if (!MDNS.begin(myHostname)) {
          sprint(0, "Error setting up MDNS responder! (PLACED INSIDE WIFI CONNECTED BLOCK)", myHostname);
        } else {
          sprint(2, "mDNS responder started", myHostname);
          // Add service to MDNS-SD
          MDNS.addService("http", "tcp", 80);
        }

        //////////////////////////////////////////////////////

       /* 
        * Set a unique client id to connect to mqtt broker (client prefix + node's mac)
        * This assignement needs to be done after succesful connection to wifi (WITH INTERNET TESTED!)
        * since we need the wifi module fully operational to query its MAC
        * Also, it needs to be assigned before connecting to mqtt brokers
        * If MAC is not yet populated, client IDs from differnt modules will be the same (just the common preffix)
        * and will disconnect each other when trying to connect to a broker
        */       
        strcpy(mqttClientId, mqttClientPrefix);
        strcat(mqttClientId, nodeId);
       
        /* get public IP */
        HTTPClient http;
        //////////////////////////////////////////////////////////
        /////// >>> MAKE THIS SERVICE SITE CONFIGURABLE VIA MQTT
        //////////////////////////////////////////////////////////
        http.begin("http://api.ipify.org/?format=text");
        int httpCode = http.GET(); //Send the request
        if (httpCode > 0){ /* internet is up */
          /* convert const String (http.getString)to const char* (wanIp) */
          //http.getString().toCharArray(wanIp, http.getString().length()+1);
          //another way:
          sprintf(wanIp, "%s", http.getString().c_str());
          sprint(2,"Public IP", wanIp);
          internetUp = true;
        }else{
        /*
         * No internet access - reset flags
         * 
         * if http.get return code is less than zero, asume internet down
         * https://github.com/esp8266/Arduino/issues/5137#issue-360559415
         * ////////////////////////////////////////////////////////////////////
         * try other wifi APs (including the default for factory testing)
         */
          sprint(0, "WiFI is Up but no Internet",); 
          internetUp = false;
          wifiUp = false;
        }
        http.end();   //Close connection  
      }else {
          /* Not yet connected to WiFI - Clear IPs */
          strcpy(wanIp, "0.0.0.0");
          resetMqttBrokerStates(); /* set brokers' status in failed mode - brokers are tested by mqtt/manageMqtt()*/
          WiFi.disconnect();
       }
    }
    /* wifi status has not changed since the previous loop */
    if (currentWifiState == WL_CONNECTED) {
      //////// WHAT IS THIS FOR??????????
      // Do we need internet access for tis update to work?
      /////////////////////////////////
      MDNS.update();
    }
 }


//THIS FUNCTION MIGHT NEED TO BE CHANGED
//IT CONLY CONNECTS TO ONE AP
//NEEDS TO CHECK TWO SSIDs WITH POSSIBLY MULTIPLE APs WITH THE SAME SSID
// AND FACTORY TEST SSID 
//Do we want to make the factory ssid match the node mac as password? That way it is unique!

/*
 * connectWlan()
 * 
 * Attempts to connect to the provided ssid
 * Resets the timer to start another reconnect cycle
 */
 void connectWlan() {
  sprint(1, "---------------------------------------",);
  sprint(1,wifiUp, internetUp); /* print both status flags */
  sprint(1, "---------------------------------------",);
  sprint(2, "WiFi Client Connect Request to", cfgSettings.ap_ssid); 
  lastWifiState = WL_IDLE_STATUS; /* reset current wifi state flag to force checking internet access by fetching the public IP*/  
  resetMqttBrokerStates(); /* Since we are re/connecting to wifi, set mqtt brokers status offline */ 
  sprint(2, "Connecting to WiFi AP", cfgSettings.ap_ssid);
  WiFi.disconnect(); /* Disconnect first from current AP, if connected */
  WiFi.begin(cfgSettings.ap_ssid, cfgSettings.ap_pswd); /* connect to the ssid/pswd in ram */
  int connStatus = WiFi.waitForConnectResult(); /* Wait here until module connects to the access point, or times out*/
  sprint(1, "connectWifi() Result", wifiStates[connStatus]);
  wifiUp = (WiFi.status() == WL_CONNECTED) ? true : false; /* set wifi state flag */  
  lastConnectTry = millis(); /* restart timer */
 } 
 
