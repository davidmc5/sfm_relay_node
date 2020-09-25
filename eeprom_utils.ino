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
  /* cfgStartAddr is the starting address of the struct in the eeprom */ \
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
  sprint(0, "SAVING WIFI AND FIRST RUN FLAG (EEPROM_UTILS)",)
}

/*
 * Read a null terminated string
 * starting at an arbitrary offset (between 0 and 512)
 * and up to a max length (if no null character found
 * Place the string in tempBuffer array
 */
void readFlashString(int addr, int maxLength){
  int i;
  int len=0;
  unsigned char k;
  EEPROM.begin(512);
  k=EEPROM.read(addr);
  
  while(k != '\0' && len<maxLength){   //Read until null character
    k=EEPROM.read(addr+len);
    tempBuffer[len]=k;
    len++;
  }
  tempBuffer[len]='\0';
  EEPROM.end();
  yield();
}
