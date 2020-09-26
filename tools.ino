//////////////////////////////////////////
//////////////////////////////////////////
// THESE USE String OBJECTS
//CONVERT TO REGULAR STRING ARRAYS.
//////////////////////////////////////////
//////////////////////////////////////////

/** Is this an IP? */
boolean isIp(String str) {
  for (size_t i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

/** IPAddress to String? */
/* IPAddress object is an array of 4 integers: [192, 168, 1, 1] */
String toStringIp(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

void removeChar( char* str, char t ){
  /* https://stackoverflow.com/a/7822057 */
  int i,j;
  i = 0;
  while(i<strlen(str))
  {
      if (str[i]==t) 
      { 
          for (j=i; j<strlen(str); j++)
              str[j]=str[j+1];   
      } else i++;
  }
}



///////////
//temporary function to retrieve ssid/pswd after a fw upgrade before changing the configuration settings. 

/*
 * Steps -- on setup before anything else takes place...
 * 1) check if 'ok' is in the expected position. If not, check if new position. else ?
 * 2) retrieve current ssid and pswd
 * 3) retrieve from file the broker's settings
 * 4) store config settings to new structure 
 */

 

////////////////////////
// USE saveWifiCredentials() to store.
////////////////////////////////////////////////////
void fixSettings(){
  /*
   * Note: Currently only migrate wifi ssid/pswd
   * the rest of settings are lost
   * The reason for this fix is that we moved the conf struct from offset 100 to offset 0 on the flash
   * also we moved the first run flag from the third to the first object on the struct
   * With this reordering, after an upgrade to this release, the node would not be able to find the configured ssid/pswd 
   * and would fail to connect to wifi
   */
  /* check if we can read current firstRun flag location */
  getCfgSettings();
  if ( strcmp(cfgSettings.firstRun, "OK") != 0){
    /* 
     *  Flag not found at current location
     *  Find and migrate wifi settings 
     *  so unit can connect to wifi after upgrade
     */
    //Fix for fw = 1.18
    //cfgStartAddr 100
    //char firstRun[3]; start addr: 100
    //char ap_ssid[20]; start addr: 103
    //char ap_pswd[20]; start addr: 123
    int startAddr = 100;
    readFlashString(startAddr, 3);
    if (strcmp(tempBuffer, "OK")==0){
      /* migrate ssid */
      readFlashString(startAddr+3, 20);    
      strcpy(cfgSettings.ap_ssid, tempBuffer);
      /* migrate pswd */
      readFlashString(startAddr+23, 20);    
      strcpy(cfgSettings.ap_pswd, tempBuffer);
      saveWifiCredentials();
    }
    
    //fix for fw < 1.18
    //cfgStartAddr 100
    //char ap_ssid[32]; start addr: 100
    //char ap_pswd[32]; start addr: 100+32 = 132
    //char ap_ok[3];    start addr: 132+32 = 164
    else{
      startAddr = 100;
      readFlashString(startAddr+64, 3);
      if (strcmp(tempBuffer, "OK")==0){ 
        /* migrate ssid */
        readFlashString(startAddr, 32);    
        strcpy(cfgSettings.ap_ssid, tempBuffer);
        /* migrate pswd */
        readFlashString(startAddr+32, 32);    
        strcpy(cfgSettings.ap_pswd, tempBuffer);
        saveWifiCredentials();
      }
    }
  }
}
