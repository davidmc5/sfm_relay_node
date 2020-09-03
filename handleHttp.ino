
/** Handle root or redirect to captive portal */
void handleRoot() {
  if (captivePortal()) { // If captive portal redirect instead of displaying the page.
    return;
  }
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");

  /* 
   * F(string_literal) is an arduino macro to store literal strings to flash instead of defaulting to ram
   * https://stackoverflow.com/a/39658069
   */   
//  String Page;
//  Page += F(
//            "<html><head></head><font size = '50'></font><body>"
//            "<div style='font-size:30px;'>"
//            "<font size = \"10\"></font>"
//            "<h1>WiFi Client Configuration!!</h1>"
//            );
//            
//  if (server.client().localIP() == apIP) {
//    Page += String(F("<p>Connected to: ")) + softAP_ssid + F("</p>"); /* Node's AP */
//  } else {
//    Page += String(F("<p>Connected to: ")) + cfgSettings.ap_ssid + F("</p>"); /* External AP */
//  }
//  Page += F(
//            "<p><a href='/wifi'>Configuration</a>.</p>"
//            "</body></html>"
//            );
//  server.send(200, "text/html", Page);

}

/*
 * Redirect to captive portal if we got a request for another domain. 
 * Return true so the page handler does not try to handle the request again. 
 */
boolean captivePortal() {
  if (!isIp(server.hostHeader()) && server.hostHeader() != (String(myHostname) + ".local")) {
    sprint(2, "Request redirected to captive portal",);
    server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
    server.send(302, "text/plain", "");   // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}

//////////////////////////////////////////////
//////////////////////////////////////////////
//////////////////////////////////////////////

/** Wifi config page handler */
void handleWifi() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");

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
            "<tr><td>SSID: "
      )
    )+
    String(cfgSettings.ap_ssid) +
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
              "<input type='text' placeholder='ssid' name='n'/>"
              "<br/><input type='password' placeholder='password' name='p'/>"
              "<br/><input type='submit' value='Connect'/></form>"
            "</div></body></html>"
           );



  /* LAN and WLAN ssid and IP */
//  Page +=
//    String(F(
//             "\r\n<br />"
//             "<table><tr><th align='left'>SoftAP config</th></tr>"
//             "<tr><td>SSID ")) +
//    String(softAP_ssid) +
//    F("</td></tr>"
//      "<tr><td>IP ") +
//    toStringIp(WiFi.softAPIP()) +
//    F("</td></tr>"
//      "</table>"
//      "\r\n<br />"


//      "<table><tr><th align='left'>WLAN config</th></tr>"
//      "<tr><td>SSID ") +
//    String(cfgSettings.ap_ssid) +
//    F("</td></tr>"
//      "<tr><td>IP ") +
//    toStringIp(WiFi.localIP()) +
//    F("</td></tr>"
//      "</table>"
//      "\r\n<br />"
//      "<table><tr><th align='left'>WLAN list (refresh if any missing)</th></tr>");


//  if (server.client().localIP() == apIP) {
//    Page += String(F("<p>You are connected through the soft AP: ")) + softAP_ssid + F("</p>");
//  } else {
//    Page += String(F("<p>You are connected through the wifi network: ")) + cfgSettings.ap_ssid + F("</p>");
//  }

  sprint(2, "WiFi Scan",);
  ////////////////////////////////////////
  ///ADD HIPERLINKS TO EACH NETWORK 
  ////////////////////////////////////////
 
  int n = WiFi.scanNetworks();
  
  if (n > 0) {
    for (int i = 0; i < n; i++) {
      Page += String(F("\r\n<tr><td>SSID ")) + WiFi.SSID(i) + ((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? F(" ") : F(" *")) + F(" (") + WiFi.RSSI(i) + F(")</td></tr>");
    }
  } else {
    Page += F("<tr><td>No WLAN found</td></tr>");
  }

////"<p>You may want to <a href='/'>return to the home page</a>.</p>"

  /* Send the page to browser */
  server.send(200, "text/html", Page);
  server.client().stop(); // Stop is needed because we sent no content length
}

/** Handle the WLAN save form and redirect to WLAN config page again */
void handleWifiSave() {
//  Serial.println("wifi save");
  sprint(2, "WiFi Save",);

  server.arg("n").toCharArray(cfgSettings.ap_ssid, sizeof(cfgSettings.ap_ssid) - 1);
  server.arg("p").toCharArray(cfgSettings.ap_pswd, sizeof(cfgSettings.ap_pswd) - 1);
  server.sendHeader("Location", "wifi", true);
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(302, "text/plain", "");    // Empty content inhibits Content-length header so we have to close the socket ourselves.
  server.client().stop(); // Stop is needed because we sent no content length
  saveWifiCredentials();
  connect = strlen(cfgSettings.ap_ssid) > 0; // Request WLAN connect with new credentials if there is a SSID
}

void handleNotFound() {
  if (captivePortal()) { // If caprive portal redirect instead of displaying the error page.
    return;
  }
  String message = F("File Not Found\n\n");
  message += F("URI: ");
  message += server.uri();
  message += F("\nMethod: ");
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += F("\nArguments: ");
  message += server.args();
  message += F("\n");

  for (uint8_t i = 0; i < server.args(); i++) {
    message += String(F(" ")) + server.argName(i) + F(": ") + server.arg(i) + F("\n");
  }
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(404, "text/plain", message);
}
