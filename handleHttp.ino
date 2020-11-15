
/** Handle root or redirect to captive portal */
void handleRoot() {
  if (captivePortal()) { // If captive portal redirect instead of displaying the page.
    return;
  }
  httpServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  httpServer.sendHeader("Pragma", "no-cache");
  httpServer.sendHeader("Expires", "-1");

  /* 
   * F(string_literal) is an arduino macro to store literal strings to flash instead of defaulting to ram
   * https://stackoverflow.com/a/39658069
   */   
}

/*
 * Redirect to captive portal if we got a request for another domain. 
 * Return true so the page handler does not try to handle the request again. 
 */
boolean captivePortal() {
  if (!isIp(httpServer.hostHeader()) && httpServer.hostHeader() != (String(myHostname) + ".local")) {
    sprint(2, "Request redirected to captive portal",);
    httpServer.sendHeader("Location", String("http://") + toStringIp(httpServer.client().localIP()), true);
    httpServer.send(302, "text/plain", "");   // Empty content inhibits Content-length header so we have to close the socket ourselves.
    httpServer.client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}

//////////////////////////////////////////////
//////////////////////////////////////////////
//////////////////////////////////////////////

/** Wifi config page handler */
void handleWifi() {
  httpServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  httpServer.sendHeader("Pragma", "no-cache");
  httpServer.sendHeader("Expires", "-1");

  String Page;

  /* CSS Style */
  Page += F(
            "<html>"
              "<head><style>"
                "body{color: black;}"
                "p{text-align: center; font-size: 70px;}"
                "div{font-size: 70px; padding: 20px 20px 20px 40px;}"
                "input[type='text']{font-size:70px; padding-left: 20px;}"
                "input[type='password']{font-size:70px; padding-left: 20px;}"
                "input[type='submit']{font-size:50px; padding-left: 20px;}"
                "table{font-size:50px; padding: 20px 20px 20px 40px;}"
              "</style></head>"
            "<body>"            
            );

  /* Identify node ID on the page header */
  Page += String(F("<p>")) + softAP_ssid + F("</p>");


  /* Current SSID and IPs */
  Page +=
    String(
      F(
         "\r\n<br />"
         "<table>"
            "<tr><td>FW Ver: "
      )
    )+
    FW_VERSION +

    F(
      "</td></tr>"
      "<tr><td>SSID: "
    )+
    WiFi.SSID() +
    F(
      " ("
    )+
    WiFi.RSSI() +
    F(
      ")"
    )+    
    F(
      "</td></tr>"
      "<tr><td>LAN: "
    )+
    toStringIp(WiFi.localIP()) +
    
    F(
      "</td></tr>"
      "<tr><td>WAN: "
    )+
    wanIp +
    
    F(
      "</td></tr>"
      "</table>"
      "\r\n<br />"
    );


 /* Get ssid and password from user */
  Page += F(
            "<div>"
              "<form method='POST' action='wifisave'>"
              "<input type='text' placeholder='ssid' name='n' id='ssid' />"
              "<br/><input type='password' placeholder='password' name='p' id='pswd' />"
              "<br/><input type='submit' value='Connect'/></form>"
            "</div>"
           );




  sprint(2, "WiFi Scan",);
  ////////////////////////////////////////
  ///ADD HIPERLINKS TO EACH NETWORK 
  ////////////////////////////////////////
 
  int n = WiFi.scanNetworks();
  //asynchronous mode //
  //https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/scan-examples.html
  //int n = WiFi.scanNetworks(true);

  ///////////////////////////

  Page +=
    String(
      F(
         "\r\n<br />"
         "<table>"
      )
    );
    


  //////////////////////////
  /// Page += String(F("\r\n<tr><td>")) + WiFi.SSID(i) + F("  (") + WiFi.RSSI(i) + F(")</td></tr>");
  /// Page += String(F("\r\n<tr><td><a href='#pswd' onclick='on_click()'>")) + WiFi.SSID(i) + F("  (") + WiFi.RSSI(i) + F(")</a></td></tr>");
  
  if (n > 0) {
    for (int i = 0; i < n; i++) {
      //http.getString().toCharArray(wanIp, len+1);
      //WiFi.SSID(i).toCharArray(tempBuffer, WiFi.SSID(i).length()+1);
      //sprint(2, "SSID", tempBuffer);
      
      Page += String(F("\r\n<tr><td><a href='#ssid'"))+
    
      F(
        "\">"
      )+ 
             
      WiFi.SSID(i) + 
      F("  (") + WiFi.RSSI(i) + 
      F(")</a></td></tr>");
    }
  } else {
    Page += F("<tr><td>No WLAN found</td></tr>");
  }
  
  Page +=
  String(
    F(
      "</table>"
      "\r\n<br />"
    )
  );

/// "function on_click(clicked_ssid){document.getElementById('ssid').value = clicked_ssid}</script>"
/// "function on_click(){document.getElementById('ssid').value='pepe'}</script>"
////"<p>You may want to <a href='/'>return to the home page</a>.</p>"

  /* Send the page to browser */
  httpServer.send(200, "text/html", Page);
  httpServer.client().stop(); // Stop is needed because we sent no content length
}


//THIS FUCTION CAN ONLY BE CALLED IF WE ARE CONNECTED TO WIFI
/*
 * handleWifiSave()
 * 
 * Handle the WLAN save form and redirect to WLAN config page again
 * https://techtutorialsx.com/2016/10/22/esp8266-webserver-getting-query-parameters/
 * 
 * This function stores in ram (only) the ssid/pswd entered in the captive portal 
 * resets the wifiUp flag so the manageWifi fuction attempts to connect
 */
void handleWifiSave() {
  sprint(2, "WiFi Save", cfgSettings.ap_ssid);
  /* save the given ssid to ram struct field */
  httpServer.arg("n").toCharArray(cfgSettings.ap_ssid, sizeof(cfgSettings.ap_ssid) - 1);
  /* save the given wifi password to ram struct field */
  httpServer.arg("p").toCharArray(cfgSettings.ap_pswd, sizeof(cfgSettings.ap_pswd) - 1);
  httpServer.sendHeader("Location", "wifi", true);
  httpServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  httpServer.sendHeader("Pragma", "no-cache");
  httpServer.sendHeader("Expires", "-1");
  httpServer.send(302, "text/plain", "");    // Empty content inhibits Content-length header so we have to close the socket ourselves.
  httpServer.client().stop(); // Stop is needed because we sent no content length
  wifiUp = false; /* set the flag to connect to the given access point */
  /* ram and flash settings are now different. Set the flags for unsaved changes */
  wifiConfigChanges = true; /* request to save since wifi settings are valid */
  unsavedChanges = true; /* this flag is reset by eeprom/saveAll() */
}

void handleNotFound() {
  if (captivePortal()) { // If caprive portal redirect instead of displaying the error page.
    return;
  }
  String message = F("File Not Found\n\n");
  message += F("URI: ");
  message += httpServer.uri();
  message += F("\nMethod: ");
  message += (httpServer.method() == HTTP_GET) ? "GET" : "POST";
  message += F("\nArguments: ");
  message += httpServer.args();
  message += F("\n");

  for (uint8_t i = 0; i < httpServer.args(); i++) {
    message += String(F(" ")) + httpServer.argName(i) + F(": ") + httpServer.arg(i) + F("\n");
  }
  httpServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  httpServer.sendHeader("Pragma", "no-cache");
  httpServer.sendHeader("Expires", "-1");
  httpServer.send(404, "text/plain", message);
}
