/*
 * User-reserved flash is 512 bytes
 * 
 * ******************************
 */


/// Store a single arbitrary struct member M in FLASH
//https://www.embedded.com/learn-a-new-trick-with-the-offsetof-macro/
//https://stackoverflow.com/questions/53232911/arduino-esp8266-read-string-from-eeprom
#define SAVECFG(M)\
  EEPROM.begin(512);  \
  /* Calculate the starting eeprom address of the given structure member M */ \
  /* apCfgStart is the starting address of the struct in the eeprom */ \
  /* offsetof(apStruct_t, M)) is the member offset from the start of the struct; */ \
  EEPROM.put(cfgStartAddr+offsetof(cfgSettings_s, M), cfgSettings.M);   \
  delay(200);\
  EEPROM.end();\
  //Example to store the struct member 'ap_pswd': 
  //SAVECFG(ap_pswd);
  


void getCfgSettings(){
  /* retrievse all config settings on eeprom into cfgSettings struct in ram */
  EEPROM.begin(512);
  EEPROM.get(cfgStartAddr, cfgSettings);
  EEPROM.end();
 }


void loadWifiCredentials() {
  getCfgSettings();
    
  /* if it is the first time to connect to wifi (no "OK" stored), ignore current eeprom contents */  
  if ( strcmp(cfgSettings.firstRun, "OK") != 0){
    strcpy(cfgSettings.ap_ssid, "<no ssid>");
    strcpy(cfgSettings.ap_pswd, "<no password>");
  }
  sprint(2, "Using WiFI Client Credentials for", cfgSettings.ap_ssid);
  sprint(2, "Password", cfgSettings.ap_pswd);
}

void saveWifiCredentials() {
  SAVECFG(ap_ssid);
  SAVECFG(ap_pswd);
  strcpy(cfgSettings.firstRun, "OK");
  SAVECFG(firstRun);
}
