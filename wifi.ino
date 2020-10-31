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
  delay(200); // Without delay I've seen the IP address blank
  sprint(2, "SoftAP IP Address", WiFi.softAPIP()); 
  
  /* Setup the DNS server redirecting all the domains to the apIP */
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  
  /*
   * if DNSServer is started with "*" for domain name, it will reply with
   * provided IP to all DNS request
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
  /* Request to Connect (TRUE) to Access Point if there is a valid SSID (non-zero length) */
  connect = strlen(cfgSettings.ap_ssid) > 0; 
 }



/*
 * manageWifi()
 * 
 * Checks if connected to wifi
 * if not, attempt to connect
 * Also connect if the 'connect' flag has been set
 * this implies a request to test if the current ssid/pswd on ram are correct.
 */

/* manage connection to AP */
 void manageWifi(){
  if (WiFi.status() != WL_CONNECTED){
    connect = true;
  }
  if (connect || !internetUp) { /* Disconnect from current AP and attempt to connect to the ssid on the current ram settings */  
    connectToWifiAp();    
//    sprint(1, "---------------------------------------",);
//    sprint(1,connect, internetUp);
//    sprint(1, "---------------------------------------",);
//   
//    sprint(2, "WiFi Client Connect Request to", cfgSettings.ap_ssid); 
//    lastWifiState = WL_IDLE_STATUS; /* reset current flag to force checking internet access by getting the public IP*/  
//    connect = false; /* reset the flag back to normal to prevent continuous looping - we are also setting a timer */
//    /// SINCE WE ARE CONNECTING TO WIFI WE NEED TO SET THE MQTT BROKERS FLAGS OFFLINE
//    resetMqttBrokerStates();
//    connectWifi(); /* Disconnect if already connected and connect to the stored access point */
//    lastConnectTry = millis(); /* set a timer */
  }
  unsigned int currentWifiState = WiFi.status();
  /* If WLAN is disconnected and idle try to reconnect */
  /* Don't set retry time too low as it will interfere with the softAP operation */
//  if (s == 0 && millis() > (lastConnectTry + 15000)) {
  if (currentWifiState == 0 && millis() > (lastConnectTry + 15000)) {
    sprint(0, "RESETTING WIFI - RESTORING SETTINGS FROM FLASH",);
    /* discard all unsaved setting changes and restore previous settings from flash */
    getCfgSettings(); /* recover settings from flash */
    connect = true; /* set the reconnect flag back to retry with recovered flash settings */
  }
  //
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
        /// MDNS DOES NOT SEEM TO WORK SOMETIMES... ADDING A SMALL DELAY...
        /// and not sure now what mdns does...!?
        delay(300); //without delay does not work. Trying with some delay...
        // Setup MDNS responder
        if (!MDNS.begin(myHostname)) {
          sprint(0, "Error setting up MDNS responder! (PLACED INSIDE WIFI CONNECTED BLOCK)", myHostname);
        } else {
          sprint(2, "mDNS responder started", myHostname);
          // Add service to MDNS-SD
          MDNS.addService("http", "tcp", 80);
        }
        //////////////////////////////////////////////////////
        /////////   }
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
        if (httpCode > 0){
          /* convert const String (http.getString)to const char* (wanIp) */
          //http.getString().toCharArray(wanIp, http.getString().length()+1);
          //another way:
          sprintf(wanIp, "%s", http.getString().c_str());
          sprint(2,"Public IP", wanIp);
          internetUp = true;
          //////////////////////////////////////////////////////////
          ///// SINCE WE ARE CONNECTED TO THE INTERNET WE CAN SET HERE THE WIFI-OK-TO-SAVE-FLAG
                //// If the flash and ram settings are different, it passed the wifi test without reverting to flash.
          /////////////////////////////////////////////////////////////
          // IF RAM AND FLASH ARE DIFFERENT, SINCE WE ARE CONNECTED, IT MEANS THEY ARE GOOD
          // SO WE CAN SET THE FLAG TO WIFI-OK-TO-SAVE-SO-FAR 
          // THE SAVE FUNCTION WILL NOT BE EJECUTED UNTIL AT LEAST ONE MQTT BROKER IS ONLINE
          // 
          // NOW, DO WE NEED TO SET THIS FLAG ONLY WHEN WE CAN VERIFY INTERNET CONNECTIVITY?
          //WHAT IS WE WE SUCCEED TO CONNECT TO AN ap THAT DOES NOT HAVE INTERNET?
          //BETTER WAIT UNTIL WE CAN PING SOMETHING ON THE INTERNET
          //LIKE WHAT? THE PUBLIC IP? HOW DO WE KNOW IF WE ARE GETTING A VALID PUBLIC IP?
  
          if (wifiConfigChanges){ //// CHECK ONLY AP SSID(s) AND PSWD(s)
            sprint(1, "WIFI TEST COMPLETED SUCCESFULLY. OK TO SAVE SETTINGS",);
            //////////////////////////////////////
            // NOW IT IS UP TO THE BROKERS-CHECK FUNCTION TO DECIDE IF A SAVEALL CAN BE DONE, BASED ON THE FLAGS SET.
            //////////////////////////////////////
            //////////////////////////////////////
            // WHAT IF WE ALSO MADE BROKER CHANGES THAT ARE BAD? 
            // SAVING ALL WILL STORE INVALID SETTINGS IN FLASH 
            // THAT WILL CAUSE THE DEVICE TO NEVER CONNECT AND REQUIQRE A SITE VISIT!
            // SAVE ONLY WHEN WE HAVE FULL CONECTIVITY!!
            // INSTEAD OF SAVEALL WE COULD JUST LET ANOTER PROCESS TEST MQTT BUT ONLY IF THE TEST-SETTINGS FLAG IS SET
            // IF WIFI & INTERNET TESTS PASSED, AND CURRENTLY CONNECTED, SET FLAG TO TEST MQTT. 
            // IF IT ALSO CONNECTS, AND SETTINGS ARE DIFFERENT, THEN SAVE 
            //WHERE ARE WE TESTING MQTT? -> ON THE mqtt/manageMqtt()
            //WE SET THE MQTT TEST FLAG BY CALLING resetMqttBrokerStates()
            //////////////////////////////////////////////////////////
            http.end();   //Close connection     
        }else{
          sprint(2, "No WiFI Changes to Save",);
          }
       }else{
          // if http.get return code is less than zero, then we can't assume we are connected to the internet
          //https://github.com/esp8266/Arduino/issues/5137#issue-360559415
          //Connected to wifi but no internet access 
          // try here other wifi APs (including the default for factory testing)
          sprint(0, "WiFI is Up but No Internet Acess!",); 
          internetUp = false;
          connect = true; 
       }
      } else {
          /* Not connected to WiFI yet - Clear IPs*/
          strcpy(wanIp, "0.0.0.0");
          resetMqttBrokerStates(); /* set brokers' status in failed mode */
          if (currentWifiState == WL_NO_SSID_AVAIL) {
            WiFi.disconnect();
          }
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




 void connectToWifiAp() {
  sprint(1, "---------------------------------------",);
  sprint(1,connect, internetUp);
  sprint(1, "---------------------------------------",);
  sprint(2, "WiFi Client Connect Request to", cfgSettings.ap_ssid); 
  lastWifiState = WL_IDLE_STATUS; /* reset current flag to force checking internet access by getting the public IP*/  
  connect = false; /* reset the flag back to normal to prevent continuous looping - we are also setting a timer */
  /// SINCE WE ARE CONNECTING TO WIFI WE NEED TO SET THE MQTT BROKERS FLAGS OFFLINE
  resetMqttBrokerStates();
  //connectWifi(); /* Disconnect if already connected and connect to the stored access point */
  //lastConnectTry = millis(); /* set a timer */
  sprint(2, "Connecting to WiFi AP", cfgSettings.ap_ssid);
  WiFi.disconnect(); /* Disconnect first from current AP, if connected */
  WiFi.begin(cfgSettings.ap_ssid, cfgSettings.ap_pswd); /* connect to the ssid/pswd in ram */
  int connStatus = WiFi.waitForConnectResult();
  sprint(1, "connectWifi() Result", wifiStates[connStatus]);
  lastConnectTry = millis(); /* set a timer */
 } 
 
// void connectWifi() {
//  sprint(2, "Connecting to WiFi AP", cfgSettings.ap_ssid);
//  WiFi.disconnect(); /* Disconnect first from current AP, if connected */
//  WiFi.begin(cfgSettings.ap_ssid, cfgSettings.ap_pswd); /* connect to the ssid/pswd in ram */
//  int connStatus = WiFi.waitForConnectResult();
//  sprint(1, "connectWifi() Result", wifiStates[connStatus]);
// }
