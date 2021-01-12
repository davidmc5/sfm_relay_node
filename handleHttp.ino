
/*
 * Documentation: 
 * https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/server-examples.html
 * https://lastminuteengineers.com/creating-esp8266-web-server-arduino-ide/
 * https://techtutorialsx.com/2016/10/22/esp8266-webserver-getting-query-parameters/
 * 
 * F(string_literal) is an arduino macro to store literal strings to flash instead of defaulting to ram
 * https://stackoverflow.com/a/39658069
 */

/*
 * configPage()
 * 
 * Returns the html page to configure the client wifi
 */

String configPage(){
    String Page;

  /* CSS Style */
  Page += F(
            "<html>"
              "<head>"
              "<style>"
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

  ////////////////////////////////////////
  ///ADD HIPERLINKS TO EACH NETWORK 
  ////////////////////////////////////////
 
  // REPLACE VARIABLE n WITH wifiScannedIps
  int n = wifiScannedIps;
  
  //asynchronous mode //
  //https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/scan-examples.html
  ///////////////////////////

  Page +=
    String(
      F(
         "\r\n<br />"
         "<table>"
      )
    );
    
  ////////////////////////// TESTING ADDING HIPERLINK
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
      "</table></html>"
      "\r\n"
    )
  );

/// "function on_click(clicked_ssid){document.getElementById('ssid').value = clicked_ssid}</script>"
/// "function on_click(){document.getElementById('ssid').value='pepe'}</script>"
return Page;
}



void handleWifi() {
  /*
   * root configuration page
   */
  sprint(2, "SENDING CONFIGURATION PAGE TO CLIENT AT: ", httpServer.client().remoteIP());
  /*
   * The Location response header indicates the URL to redirect a page to. 
   * It is only meaningful when served with a 3xx (redirection) or 201 (POST/created) status response.
   * https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Location
   */ 
  //httpServer.sendHeader("Location", "/", true); ///////FOR TESTING
  
  httpServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  httpServer.sendHeader("Pragma", "no-cache");
  httpServer.sendHeader("Expires", "-1");
  String Page = configPage();  
//  httpServer.send(302, "text/html", Page);
  httpServer.send(200, "text/html", Page); 
  httpServer.client().stop(); 
  return;
}


void handleWifiSave() {
/*
 * Captures SSID/PSWD arguments
 * Redirects to waiting page /configWait
 * After a timeout the waiting page will redirect to root config page again
 * 
 * This function stores in ram (only) the ssid (argument = "n") and pswd (argument = "p") entered in the captive portal 
 * sets retryWifiFlag so the wifi/manageWifi function attempts to connect to verify credentials are valid
 * https://techtutorialsx.com/2016/10/22/esp8266-webserver-getting-query-parameters/
 */
  /* save the given ssid ("n") to ram struct field */
  httpServer.arg("n").toCharArray(cfgSettings.apSSIDlast, sizeof(cfgSettings.apSSIDlast) - 1);
  /* save the given wifi password ("p") to ram struct field */
  httpServer.arg("p").toCharArray(cfgSettings.apPSWDlast, sizeof(cfgSettings.apPSWDlast) - 1);
  sprint(2, "WIFI CONNECT REQUEST TO: ", cfgSettings.apSSIDlast);
  sprint(2, "REQUESTING CLIENT: ", httpServer.client().remoteIP());
  /*
   * The Location response header indicates the URL to redirect a page to. 
   * It is only meaningful when served with a 3xx (redirection) or 201 (POST/created) status response.
   * https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Location
   */ 
  httpServer.sendHeader("Location", "/configWait", true);
  httpServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  httpServer.sendHeader("Pragma", "no-cache");
  httpServer.sendHeader("Expires", "-1");
  
  httpServer.send(302, "text/html", "");    // Empty content inhibits Content-length header so we have to close the socket ourselves.
  httpServer.client().stop(); // Stop is needed because we sent no content length
  /* ram and flash settings are now different. Set the flags for unsaved changes */
  unsavedChanges = true; /* Flash and ram settings are now different. This flag is reset by eeprom/saveAll() once settings are verified */
  wifiConfigChanges = true; /* save settings request if wifi settings are verified to be valid */
  retryWifiFlag = true; /* test new AP settings immediately */
}


void handleConfigWait(){
  String page = updatePage();
  httpServer.sendHeader("Location", "/", true);
  httpServer.send(200, "text/html", page);  
  httpServer.client().stop(); // Stop is needed because we sent no content length
}

String updatePage(){
    String Page;
  /* CSS Style */
  Page = F(
            "<html>"
              "<head>"
                "<meta http-equiv='refresh' content='10 url= /'/>"
              "<style>"
                "body{color: black;}"
                "h1{font-size: 100px; padding: 20px 20px 20px 40px;}"
              "</style></head>"
            "<body>"
              "<h1>Updating Settings...</h1>"    
            "</body>"       
            );
  return Page;
}
