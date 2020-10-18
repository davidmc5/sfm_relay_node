/*
 * saveAll()
 * 
 * Saves all the current settings in RAM to FLASH
 * First set the value(s) in ram using mqtt/setSetting()
 */
void saveAll(){
  sprint(1,"SAVING CONFIGURATION SETTINGS TO FLASH -- eeprom/saveAll()",);
  EEPROM.begin(512);
  EEPROM.put(cfgStartAddr, cfgSettings);
  delay(200);
  EEPROM.end();
  unsavedChanges = false; /* ram and flash settings now match */
}



/* 
 *  getCfgSettings()
 *  Retrieves ALL config settings on eeprom into the given struct in ram
 */
void getCfgSettings(struct cfgSettings_s *structSettings){
  EEPROM.begin(512);
  EEPROM.get(cfgStartAddr, *structSettings);
  EEPROM.end();
  delay(200);
 }



/*
 * STRNCOPY(D, S)
 * stringCopy()
 * Safe string copy to prevent and alert of buffer overrun
 */
  //////////////////////////////////////////////////////////////
  ////////// NEED TO CHECK THE SIZE OF THE SOURCE ALSO
  ////////// IF THE SOURCE DOES NOT HAVE A NULL TERMINATIONS, 
  ////////// WE MIGHT BE READING CHARACTERS PAST THE SIZE OF THE SOURCE!
  /////////////////////////////////////////////////////////////////////////

#define STRNCOPY(D, S) (stringCopy(D, S, sizeof(D)))
int stringCopy(char* dest, char* source, int dest_size){
  // calculate the length of the source string
  int c = 0;
  int ret=0;
  if (dest_size > 0){
    dest[0] = '\0'; /* set an initial null to concatenate */    
    /* find length of source string */
    while(source[c] != '\0')
       c++;
    /* check that source string fits in destination array */
    if (dest_size >= c+1){ /* Enough room to save source string */
      strncat(dest, source, dest_size);
    }else{
      sprint(0, "EXCEEDS DESTINATION SIZE", source);
      ret = 1; /* return truncation flag */
    }
  }
  return ret;
}


/*
 * saveSettings()
 * Save configuration settings to flash
 */
void saveSettings(){
  /* get settings from flash into temp struct to compare with ram settings */
  EEPROM.begin(512);
  EEPROM.get(cfgStartAddr, cfgSettingsTemp);
  //delay(200); ////////////////////////////////////
  /* 
   *  Access each struct field using: base_struct_address + field_offset
   *  https://stackoverflow.com/a/2043949
   */  
  //  //FOR TESTING - Make RAM setting different than FLASH
  //  STRNCOPY(cfgSettingsTemp.debug, "supper long string"); //changing flash struct field
  //  STRNCOPY(cfgSettings.debug, "hula"); //change ram struct field
  
  //clear field
  // cfgSettingsTemp.debug[0]='\0';
  // cfgSettingsTemp.mqttUserB[0]='\0';
 
  int comp;
  int i=0;
  for (i = 0; i<numberOfFields; i++){
    if ( checkConfigSettings(structFlashBase+field[i].offset, field[i].size) ){
      /* field data string is longer than field size! */
      (structRamBase+field[i].offset)[0] = '\0'; /* delete invalid (too long) contents by adding null to the first element*/
    }
//    sprint(1, field[i].name, structRamBase+field[i].offset );  //delete this when finished testing!///////////////////////////////////////////////////
//    sprint(1, field[i].name, structFlashBase+field[i].offset );  //delete this when finished testing!///////////////////////////////////////////////////
    comp = strcmp(structFlashBase+field[i].offset, structRamBase+field[i].offset);
    if (comp != 0){ /*whats on RAM is different than what's on flash */
      sprint(1, "FIELD", field[i].name);
      sprint(1, "VALUE IN RAM", structRamBase+field[i].offset);
      sprint(1, "VALUE IN FLASH", structFlashBase+field[i].offset);      
      updateFlashField(structRamBase+field[i].offset, cfgStartAddr+field[i].offset, field[i].size);
    }
    yield(); //avoid tripping the esp8266 watchdog timer if the loop takes too long
  }
  EEPROM.get(cfgStartAddr, cfgSettingsTemp); //retrieve current flash settings to verify
  EEPROM.end(); /* this commits changes to flash and frees the flash cache in ram */ 
}

/* 
 *  updateFlashField() ///////// DO WE NEED THIS?? PROBABLY BETTER JUST STORING THE ENTIRE STRUCT FROM RAM!! 
 *  /////////////////////////////////////////////////////////////
 *  Writes each byte of the source string to flash
 *  until either a null terminator or the fieldSize are reached
 *  If the fieldSize is reached withour a null terminator, add a null and set return error
 *  
 *  Returns 0 if success or 1 if the source string was too long
 *  
 *  IMPORTANT: This function needs to be called between EEPROM.begin() and a EEPROM.end() statements.
 */  
int updateFlashField(char* sourceStr, int flashAddress, int fieldSize){
  int i;
  int endFound=0;
  int ret=0;
  for(i=0; i < fieldSize; i++){
    EEPROM.write(flashAddress+i, sourceStr[i]);
    if (sourceStr[i] == '\0'){ /* string end found */
      endFound = 1;
      break; /* end of string */
    }
  }
  if (!endFound){ /*string null terminator not found */
    EEPROM.write(flashAddress+i-1, '\0'); /* add null terminator */
    ret = 1; /* Set return flag to error*/
    sprint(1, "FLASH DATA TRUNCATED", sourceStr);
  }  
  return ret;
}


/*
 * checkConfigSettings() 
 * Checks that the string length of each struct field
 * is less or equal than the field size 
 * It looks for a null character in the string before reaching the fieldSize
 */
int checkConfigSettings(char* sourceStr, int fieldSize){
   int c = 0;
   int ret = 0;
   while(sourceStr[c] != '\0')
     c++;
   if (c >= fieldSize){
     /* send an alert */ 
      sprint(0, "CONFIGURATION SETTINGS INCORRECT", sourceStr);
      sprint(1, "Next field could be invalid too", );
      ret = 1; /* return error flag */
   }
   return ret; 
 }



/*
 * readFlashString()
 * Read a null terminated string from flash
 * starting at an arbitrary offset (between 0 and 512)
 * and up to a max length (if no null character found)
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
}


/////////////////////////////
/// OLD -- TO DELETE
///////////////////////////

//void loadWifiCredentials() {
//  /* if it is the first time to connect to wifi (no "OK" stored), ignore current eeprom contents */  
//  if ( strcmp(cfgSettings.firstRun, "OK") != 0){
//    strcpy(cfgSettings.ap_ssid, "<no ssid>");
//    strcpy(cfgSettings.ap_pswd, "<no password>");
//  }
//  sprint(2, "Connecting to WiFi", cfgSettings.ap_ssid);
//}


/*
//Convert this function to save all settings that have changed
//TODO: CHECK CURRENT FLASH CONTENTS. DO NOT WRITE IF THEY ARE THE SAME
*/
//void saveWifiCredentials() {
//  SAVECFG(ap_ssid);
//  SAVECFG(ap_pswd);
//  strcpy(cfgSettings.firstRun, "OK");
//  SAVECFG(firstRun);
//  sprint(1, "SAVING WIFI AND FIRST RUN FLAG (EEPROM_UTILS)",);
//}


/*
 * SAVECFG(M)
 * Macro to store a single struct field M in FLASH
 * Flash Size of ESP-WROOM-02 is 512 bytes
 * https://www.embedded.com/learn-a-new-trick-with-the-offsetof-macro/
 * https://stackoverflow.com/questions/53232911/arduino-esp8266-read-string-from-eeprom
 */
//#define SAVECFG(M)\
//  EEPROM.begin(512);  \
//  /* Calculate the starting eeprom address of the given structure member M */ \
//  /* cfgStartAddr is the starting address of the struct in the eeprom */ \
//  /* offsetof(apStruct_t, M)) is the member offset from the start of the struct; */ \
//  EEPROM.put(cfgStartAddr+offsetof(cfgSettings_s, M), cfgSettings.M);   \
//  delay(200);\
//  EEPROM.end();\
//  //Example to store the struct member 'ap_pswd': 
//  //SAVECFG(ap_pswd);
