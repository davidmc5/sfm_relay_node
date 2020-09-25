void startAP(){

  //starts access point and web server
  
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
  delay(500); // Without delay I've seen the IP address blank
  sprint(2, "SoftAP IP Address", WiFi.softAPIP()); 
  
  /* Setup the DNS server redirecting all the domains to the apIP */
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  
  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);



//// THIS IS NOT *ONLY* PART OF THE SOFT AP. 
///  MOVE IT TO THE SETUP()?
//// this webserver shows on both softap and wlan interface
  /*
   * Setup web pages: 
   * *root
   * *wifi config
   * *captive portal detectors
   * *not found.
   */
//  server.on("/", handleRoot);
  server.on("/", handleWifi);
  server.on("/wifi", handleWifi);
  server.on("/wifisave", handleWifiSave);
//  server.on("/generate_204", handleRoot);  //Android captive portal. Maybe not needed. Might be handled by notFound handler.
//  server.on("/fwlink", handleRoot);  //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
  server.onNotFound(handleNotFound);
  
  server.begin(); // Web server start
  sprint(2, "HTTP server started",);

  loadWifiCredentials(); // Load WLAN credentials from flash
  connect = strlen(cfgSettings.ap_ssid) > 0; // Request WLAN connect (TRUE) if there is a SSID (of non-zero length)
 }

/* manage connection to AP */
 void manageWifi(){
  if (connect) {    
    sprint(2, "WiFi Client Connect Request to", cfgSettings.ap_ssid);    
    connect = false;
    connectWifi(); /* connect to the stored access point */
    lastConnectTry = millis();
  }    
  unsigned int s = WiFi.status();
  /* If WLAN disconnected and idle try to reconnect */
  /* Don't set retry time too low as it will interfere with the softAP operation */
  if (s == 0 && millis() > (lastConnectTry + 60000)) {
    connect = true;
  }

  if (wifiStatus != s) {
    sprint(2, "WiFi Status Changed From", wifiStates[wifiStatus]);
    sprint(2, "To WiFi Status of", wifiStates[s]);
    
    wifiStatus = s;
    if (s == WL_CONNECTED) {
      /* Just connected to WLAN */
      sprint(2, "WiFI Client Connected To", cfgSettings.ap_ssid);
      sprint(2, "WiFi Client IP Address", WiFi.localIP());

      // Setup MDNS responder
      if (!MDNS.begin(myHostname)) {
        sprint(0, "Error setting up MDNS responder!", myHostname);
      } else {
        sprint(2, "mDNS responder started", myHostname);
        // Add service to MDNS-SD
        MDNS.addService("http", "tcp", 80);
      }


     /* 
      * Set a unique client id to connect to mqtt broker (client prefix + node's mac)
      * This assignement needs to be done after succesful connection to wifi 
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
      }
      http.end();   //Close connection
    
    } else {
        /* Not connected to WiFI - Clear IPs*/
        strcpy(wanIp, "0.0.0.0");
        if (s == WL_NO_SSID_AVAIL) {
          WiFi.disconnect();
        }
      }
    }
    if (s == WL_CONNECTED) {
      MDNS.update();
    }
 }


 void connectWifi() {
  sprint(2, "Connecting to WiFi AP", cfgSettings.ap_ssid);
  
  WiFi.disconnect();
  WiFi.begin(cfgSettings.ap_ssid, cfgSettings.ap_pswd);
  
  int connStatus = WiFi.waitForConnectResult();
  sprint(2, "WiFi Connect Result", wifiStates[connStatus]);
}
