
/////////////// REFERENCES //////////////////
//https://arduino-esp8266.readthedocs.io/en/latest/libraries.html
//ESP.restart() restarts the CPU.
//ESP.getFreeHeap() returns the free heap size.
//ESP.getHeapFragmentation() returns the fragmentation metric (0% is clean, more than ~50% is not harmless)
//ESP.getChipId() returns the ESP8266 chip ID as a 32-bit integer.
//ESP.getFlashChipId() returns the flash chip ID as a 32-bit integer.
//ESP.getFlashChipSize() returns the flash chip size, in bytes, as seen by the SDK (may be less than actual size).
/*
 * ESP.random() 
   * should be used to generate true random numbers on the ESP. 
   * Returns an unsigned 32-bit integer with the random number. 
   * An alternate version is also available that fills an array of arbitrary length. 
   * Note that it seems as though the WiFi needs to be enabled to generate entropy for the random numbers, otherwise pseudo-random numbers are used.
 *
 * ESP.checkFlashCRC() 
   * calculates the CRC of the program memory (not including any filesystems) 
   * and compares it to the one embedded in the image. 
   * If this call returns false then the flash has been corrupted. 
 * 
 * FUTURE: 
 *  if unable to connect to neither pri AND bak after a few retries, try the two standby brokers (hardcoded)
 */


/*
 * testSettings()
 * 
 * Test, before saving to flash
 * Verify if current settings in ram allow connecting to BOTH wifi and mqtt broker(s)
 * If unable to connect to BOTH after a number of retries, retrieve settings from flash and test again
 * If mqtt fails from both flash and ram, retrieve mqtt broker defaults
 * 
 * FUTURE: Store the last two known good wifi settings and test them in sequence during an outage
 * this will be needed also for redudant dual wifi on site 
 * (but only if backup AP has different ssid/pswd)
 */
void testSettings(){
  /* 
   *  Sets the corresponding test flags if WIFI or MQTT settings in ram are different from flash. 
   *  
   *  This function is called by
   */
  sprint (1, "VERIFYING A SAVE REQUEST",);
  if (unsavedSettingsFound()){
    if(wifiConfigChanges){
      sprint(1, "WIFI Changes Made. Testing WiFi Connectivity...",);
//      connect = true; /* force wifi to reconnect using current settings on ram */ 
      wifiUp = false; /* force wifi to reconnect using current settings on ram */ 
    }
    if (mqttConfigChanges){
      sprint(1, "MQTT Changes made. Testing mqtt Connectivity...",);
      resetMqttBrokerStates(); /* set flag for brokers down to force reconnect using current ram settings */
    }
  }else{
    sprint(1, "No Config Changes Found. Ignoring save request",);
    saveAllRequest = false; /* clear the saveAll re request */
  }
}


/*
 * unsavedSettingsFound()
 * 
 * Find if ram and flash values match
 * return 0 if match
 * return 1 if different (unsaved changes)
 */
bool unsavedSettingsFound(){
  int i;
  unsavedChanges = false;
  wifiConfigChanges = false;
  mqttConfigChanges = false;
  /* read values from flash into TEMP/FLASH struct */ 
  getCfgSettings(&cfgSettingsTemp);  
  for (i=0; i< numberOfFields; i++){
  /* Check if the value in flash is the same as value in active config in ram */
    if (strcmp(structRamBase+field[i].offset, structFlashBase+field[i].offset) != 0){
      unsavedChanges= true;
      /* check if wifi settings changed */
      if( i==1 || i==2){
        /* index 1 = ssid; index 2 = pswd (see settings.h/field struct) */
        wifiConfigChanges = true;
      }else if (i>5 && i<14) {
        /* the range of fields with index between 6 and 13 correspond to the mqtt brokers */
        mqttConfigChanges = true;        
      }
    }
    yield();
  }
  return unsavedChanges;
}




/*
 * resetMqttBrokerStates()
 * 
 * After any wifi outage
 * reinitilize state machine and 
 * set brokers offline
 * This function is called by wifi/manageWifi() when disconnected from wifi
 * and also by testSettings()
 */
void resetMqttBrokerStates(){
  mqttBrokerState = mqttBrokerUpA = mqttBrokerUpB = 0;
  wifiStatusChange = true; /////////// this could be removed if checked in main loop
}

/*
 * loadMqttBrokerDefaults()
 * 
 * Loads to the config struct the settings from mqtt_brokers file
 */
void loadMqttBrokerDefaults(){  
  /* Broker A */
  strcpy(cfgSettings.mqttServerA, mqttServer1);
  strcpy(cfgSettings.mqttPortA, mqttPort1);
  strcpy(cfgSettings.mqttUserA, mqttUser1);
  strcpy(cfgSettings.mqttPasswordA, mqttPassword1);
  /* Broker B */
  strcpy(cfgSettings.mqttServerB, mqttServer2);
  strcpy(cfgSettings.mqttPortB, mqttPort2);
  strcpy(cfgSettings.mqttUserB, mqttUser2);
  strcpy(cfgSettings.mqttPasswordB, mqttPassword2);
  sprint(0,"LOADED MQTT BROKER DEFAULTS",);
  /* connect to given brokers */
  resetMqttBrokerStates();
}


/*
 * mqttBrokerOnline()
 * 
 * Call this function when at least one broker is back online
 * Set blinking LED to solid
 * sendState publishes online flag and node status topic to EACH online broker
 */
 void mqttBrokerOnline(){
    /* Publish status message */
    sendStatus(); 
    /* Publish RETAINED online message */
    sendState(); 
 }
 

/* 
 *  manageMqtt()
 *  
 *  Implements and updates a state machine to monitor brokers
 *  Publishes different mqtt status messages depending on broker failures
 *  If both brokers are down, try to reconnect to both
 *  After a few unsuccesful retries, try default brokers
 *  If only one broker is down, don't try to recover to avoid the few seconds delay
 *  Only send status changes once.
 */
void manageMqtt(){
  /* check mqtt conection status only if connected to wifi */
  if (WiFi.status() == WL_CONNECTED && internetUp){
    /* service mqtt requests */
    mqttClientA.loop();
    mqttClientB.loop();
    /* 
     *  if a saveAll request has been made, and there are no pending wifi or mqtt changes, then save settings.
     *  Wifi and mqtt settings are critical for the node's connectivity. 
     *  These seeetings need to be tested first before saving.
     *  But if only non-critical settings are pending, saveAll can be called without testing (no risk of losing remote access to the node)
     */   
 ////////////////////////////////////////////////////
 /// NEED TO DECOUPLE SAVING LOGIC FROM MQTT ROUTINES!!!
 /////////////////////////////////////////////////////
      
    if(saveAllRequest && unsavedChanges && !wifiConfigChanges && !mqttConfigChanges){
      sprint(1, "Saving non critical config changes...",);
      saveAll();
    }
    /* monitor and update broker states */        
    switch(mqttBrokerState){       
      case 0: 
        //initial state after power up or recovery from both brokers down. Wait here until at least one broker is connected
        checkMqttBrokers();       
        if (mqttBrokerUpA || mqttBrokerUpB){ //at least one broker is up
          mqttNextState(1);
         sprint(1, "........................",);
          mqttBrokerOnline(); /* publish online status */
          sendRestart(); /* Publish node's restart code (crash message) only once after boot */ 
         sprint(1, "........................",);
          mqttErrorCounter = 0; 
          
          // IF BROKERS mqttConfigChanges FLAG IS SET -> OK TO SAVE since we are connected to wifi
          //////////////////////////////////////////////////////////////////////
          //resetMqttBrokerStates() is called from the wifi module
          //to set the brokers offline flags when re/connecting to wifi
          //
          // THE STATE MACHINE BELOW WILL LOOP/RETRY A COUPLE OF TIMES
          // AND, if unable to reconnect, INSTEAD OF SAVING, IT WILL RESTORE SETTINGS FROM FLASH
          //
          //IF WE DO NOT TRIGGER THE ERROR LOOP AND CONNECT TO AT LEAST ONE MQTT broker THEN SAVE all settings
          //BUT GIVE A WARNING AND SEND A BROKER STATUS TOPIC IF NOT CONNECTED to both brokers
          //////////////////////////////////////////////////////////////////////////
          if (wifiConfigChanges || mqttConfigChanges){
            /* we are connected to both wifi and at least to one mqtt broker -> ok to save current settings */
            sprint(1, "SAVING CONFIGURATION SETTINGS - state 0",)
            saveAll();
          }               
        }else{
          /* both brokers still down - wait here */
          mqttErrorCounter++;
          if (mqttErrorCounter > 2){
            loadMqttBrokerDefaults();
            internetUp = false; /* assume internet is down */
            wifiUp = false; /* force reconnecting to wifi */                    
            mqttErrorCounter = 0; 
          }
          sprint(1, "MQTT ERROR COUNTER >>>>>>>>>>>>> ", mqttErrorCounter);        
        }        
        break;

      case 1: 
        //Normal state - Both brokers are up - Waiting for failures
        checkMqttBrokers();
        if (!mqttBrokerUpA && !mqttBrokerUpB){ //both brokers are down. Can't send messages!
          mqttNextState(0);
        }else if(!mqttBrokerUpA || !mqttBrokerUpB){ //one broker is down
          mqttNextState(2);
          sendStatus();          
        }
        break;

      case 2: 
        // One broker failed. Wait here until full recovery or until both brokers fail
        // DO NOT check connectivity to brokers here because it adds too much blocking delay 
        // and makes the phone app unresponsive
        // reconnection attempts block the processing of mqtt messages
        if (!mqttClientA.connected()) { mqttBrokerUpA = 0;} /* node not connected to primary broker)*/
        if (!mqttClientB.connected()) { mqttBrokerUpB = 0;} /* node not connected to backup broker)*/      
        if (mqttBrokerUpA && mqttBrokerUpB){ //both brokers recovered
          mqttNextState(1);
          mqttBrokerOnline(); /* publish online status and stop LED from blinking */
        }else if (!mqttBrokerUpA && !mqttBrokerUpB){ //both brokers down - Can't send status updates
            mqttNextState(0);
        }
        mqttErrorCounter = 0;
        break;  
    }/* end of broker state machine */
  } 
} /* end of manageMqtt() */



/////////////////////////////////
/// DO WE NEED THIS?? IT IS JUST CONTROLLING THE LED!
/////////////////////////////////

/*
 * mqttNextState(state)
 * 
 * Transitions the state machine to the given state.
 * Depending on the state transition, it will start/stop blinking the led
 * to signal a failure/recovery state.
 * 
 * STATES:
 * *******
 * 0: Intial state after power up. Awaiting broker connectivity
 * 1: Normal state. Node is connected to both primary and backup brokers.
 * 2: One broker is offline. Wait here until the other one fails or a recheck/restore command changes the state
 * 3: Both brokers are offline. Monitor both brokers until at least one comes back online.
 */
void mqttNextState(int state){
  sprint(1, "STATE CHANGED FROM: ", mqttBrokerState);
  sprint(1, "STATE CHANGED TO: ", state);
  mqttBrokerState = state;
  switch(state){
    
    case(0): /* Intial State - Both brokers assumed down - Check connectivity */
      /* Blink led until broker connectivity is aserted */
      ledBlink();
      break;
      
    case(1): /* Normal State - Both mqtt brokers are online */
      /* LED on */
      ledOn();
      break;
      
    case(2): /* Only one broker is offline */
      ledOn();
      break;
  }
}










void checkMqttBrokers(){
/* 
 *  checkMqttBrokers() 
 *  
 *  checks if node is connected to each broker 
 *  and set status flags
 *  If node is not connected, attemp reconnect
 */  
  if (!mqttClientA.connected()) {   /* Test Primary mqtt broker */ 
    mqttBrokerUpA = 0; /* set broker as down */
    /*
     * Attempt to connect to Primary Broker A
     * 
     * the mqttServer and callback are set in the setup() function TASK 5
     * https://pubsubclient.knolleary.net/api#connect
     * boolean connect (clientID, [username, password], [willTopic, willQoS, willRetain, willMessage], [cleanSession])
     * mqtt username and password are defined in the file mqtt_brokers.h
     */
    sprint(1, "-------------> SETTING mqttPortA: ", cfgSettings.mqttPortA);
    ///////////////////////////////////////////////////////////////////////
    mqttClientA.setServer(cfgSettings.mqttServerA, atoi(cfgSettings.mqttPortA));  //Primary mqtt broker
    mqttClientA.setCallback(mqttCallbackA); //function executed when a MQTT message is received.
    /* Connect with a LAST WILL message payload set to 0 (false: OFFLINE) and retained */
    strcpy(mqttTopic, "status/");
    strcat(mqttTopic, nodeId);
    strcat(mqttTopic, "/state/");
    if (mqttClientA.connect(mqttClientId, cfgSettings.mqttUserA, cfgSettings.mqttPasswordA, mqttTopic, 0, true, "0" )) { 
      mqttBrokerUpA = 1;  /* connected to primary broker */
      sprint(1, "Connected to MQTT Broker A: ", cfgSettings.mqttServerA);
      /* Subscribe to all topics (/#) prefixed by this nodeId */
      strcpy(mqttTopic, nodeId);
      strcat(mqttTopic, "/#");
      mqttClientA.subscribe(mqttTopic);
      sendState(); /* update the retained online status */   
   } 
   else {   
      sprint(0, "Failure MQTT Broker A - State: ", mqttClientA.state());
   }
  } /*end of Primary mqtt broker test */
  if (!mqttClientB.connected()) {   /* Test Backup mqtt broker */
    mqttBrokerUpB = 0;       
    mqttClientB.setServer(cfgSettings.mqttServerB, atoi(cfgSettings.mqttPortB)); //Backup mqtt broker
    mqttClientB.setCallback(mqttCallbackB); //function executed when a MQTT message is received. 
    /* Set last will topic */
    strcpy(mqttTopic, "status/");
    strcat(mqttTopic, nodeId);
    strcat(mqttTopic, "/state/");    
    if (mqttClientB.connect(mqttClientId, cfgSettings.mqttUserB, cfgSettings.mqttPasswordB, mqttTopic, 0, true, "0" )) { 
      mqttBrokerUpB = 1;  /* connected to backup broker */        
      sprint(1, "Connected to MQTT Broker B: ", cfgSettings.mqttServerB);
      /* subscribe all topics for this nodeId (nodeId/#) */
      strcpy(mqttTopic, nodeId);
      strcat(mqttTopic, "/#");
      mqttClientB.subscribe(mqttTopic);
      sendState(); /* update the retained online status */  
    }
    else {   
      sprint(0, "Failure MQTT Broker B - State: ", mqttClientB.state());
    }        
  } /* end of backup broker test */
  ///////////////////////////
  if(!mqttBrokerUpA && !mqttBrokerUpB){
    internetUp = false; /* Both brokers down: assume internet failure */
  }
  ///////////////////////////
} /* end mqtt brokers check */






///MQTT CALLBACKS



/* wrappers to include the primary or backup identifiers to the received messages */
void mqttCallbackA(char* topic, byte* payload, unsigned int length){
  mqttCallback("PRI", topic, payload, length);
}
void mqttCallbackB(char* topic, byte* payload, unsigned int length){
  mqttCallback("BAK", topic, payload, length);
}




/* 
 *  mqttCallback()
 *  
 *  Called when MQTT messages for this nodeid are received from either broker
 */
void mqttCallback(char* broker, char* topic, byte* payload, unsigned int length) {
  /* send an alert if topic or payload strings are longer than buffers */
  mqttMsgCheck(topic, payload, length); 
  /* copy the received bytes from payload to mqttPayload array */
  for (int i=0; i < length; i++){
    mqttPayload[i] = (char)payload[i];
  }
  mqttPayload[length]= NULL; /* add null termination for string */
  sprint(2, "MQTT BROKER: ", broker);  
  sprint(2, topic, mqttPayload);  
  /* 
   *  Parse received mqtt topic into an array of pointers
   *  
   *  Replace the delimiters '/' with null '/0' 
   *  and store the pointer of the start of each topic substring into  topics[] 
   *  Note that the given 'topic' buffer can't be written to. 
   *  So we can't replace the '/' with a null strill termination on the returned buffer.
   *  Instead, we need to copy the received topic string to an array (rawTopics) 
   *  in order to replace the delimiters and form substrings
  */    
  /* Copy the topic string to a read/write array including the null string terminator*/
  int topic_length = strlen(topic);
  /* rawTopic[mqttMaxTopicLength] will have the same string as the given 'topic' buffer but with '/' replaced with '/0' */
  char delim = '/';
  int numTopics = 1; /*number of topics (separated by '/'). Initially assume we have at least one */
  /* remove old pointers from topics array */
  for (int i = 0; i<maxTopics; i++){
    topics[i] = NULL;
  }
  /* char *topics[maxTopics] will hold the pointers to each subtopic string */
  topics[0] = rawTopic; /* we have at least one topic's pointer */
  /* NOTE:  topics[0] IS EITHER THE NODEID(MAC) OR A BROADCAST TOPIC BUT SHOULD NEVER BE NULL*/ 
  for (int i=0 ; i <= topic_length; i++){
    /* Terminate each topic substring with a '\0' and save the pointer into the array of pointers topics[] */
    if (topic[i] == delim){
      rawTopic[i] = NULL; //end of string delimiter
      topics[numTopics++] = rawTopic+i+1; /* store the pointer to the next location after the delimeter */
    }else if(topic[i] == NULL){
      rawTopic[i] = NULL;
      break; /* we are at the end of the topic string */
    }else{
      rawTopic[i] = topic[i];
    }
  }
  /* we need at least one topic */
  if (numTopics < 2){
    sprint(1, "NO TOPICS RECEIVED!",);
    return;
  }
  /*
   * 
   * 
   * 
   * 
   * ADD BELOW THE MQTT COMMANDS FOR THE NODE
   *  
   *  
   *  
   *  
   *  
   *  RELAY CONTROL
   *  /////////////
   *  
   *  Turn a relay on or off
   */
  if(strcmp(topics[1], "relay") == 0){ 
    int relayAction = (mqttPayload[0]-'0'); //Convert to integer. 0=off, 1=on   
    setOutput(&relayState, topics[2], relayAction); //topics[2] is the relay#
  }
  /* 
   *  FIRMWARE UPDATES
   *  ////////////////
   *  
   *  Initiate a firmware upgrade. 
   *  The node will reboot and send a status update when done
   */
  else if (strcmp(topics[1], "fwupdate") == 0){
    /* msg is the mqtt payload received and must include the webserver address, port and fw filename */
    /* Example payload: http://10.0.0.200:8000/fw_rev_2.bin */
    sprint(2, "REQUESTED FIRMWARE UPGRADE: ", mqttPayload);
    t_httpUpdate_return ret = ESPhttpUpdate.update(mqttPayload);
    switch(ret) {
      case HTTP_UPDATE_FAILED:
        sprint(0, "HTTP_UPDATE_FAILED Error: ", ESPhttpUpdate.getLastErrorString().c_str());
        break;
      }
  }
  /*
   *  STATUS UPDATE
   *  /////////////
   *  
   *  Send a status update to the controller
   *  If payload is set to "1", try to reconnect to the failed broker 
   */
   else if (strcmp(topics[1], "status") == 0){
    sprint(2, "REQUESTED STATUS UPDATE: ", mqttPayload);
    if ((mqttPayload[0]-'0') == 1){
      sprint(2, "Resetting MQTT Connections: ", payload[0]-'0');
      checkMqttBrokers();
    }
    sendStatus();
  }
  /*
   * SETTINGS
   * ////////
   * 
   * NO PAYLOAD (GET): 
   *    If no specific setting on topic >> (defaul argument = 'all') publish all current settings' values in sequence
   *    If specific setting in topic >> (SINGLE) publish the CURRENT value of that setting
   *    If the given setting does not exist, publish an error
   *    
   * WITH PAYLOAD (SET):
   *    If payload, there must be a valid setting topic >> Store that setting in flash and publish confirmation
   *    RETURN A MESSAGE WITH THE UPDATED VALUE OR AN ERROR THAT NOTHING GOT CHANGED (TOO LONG/TRUNCATED).
   *    To delete a setting value enter a null character as payload
   *    
   */
   else if (strcmp(topics[1], "settings") == 0){
      if (topics[2]){ //If not a null pointer
        if (strcmp(topics[2], "saveAll") == 0){
          /* save current config values in RMA to FLASH */
          saveAllRequest = true; /* a saveAll request has been received */
          testSettings();
        }else if (strlen(topics[2])< 1) {
          /* no topic (just the string termination null character) */
          configSettings("all"); 
        }else{          
          /* publish just the requested setting. If there a payload, configure that value in ram */
          configSettings(topics[2], mqttPayload );
          sprint(1,"topics[2]: ", strlen(topics[2]));
        }
      }else{
        /* Publish all configuration settings (null pointer = no setting given */ 
        ////////////// RENAME TO sendConfigSettings() ???????????????
        configSettings("all");   
      }
   }


  /////////////////////////////////
  //// TO RESEARCH
  // THE CODE BELOW CAUSES A CRASH!
  // use it to test crash recovery
  /////////////////////////////////
  //  if ( strcmp(cmdType, "relay") == 0  ){
  //    ////the following statement causes a crash because the parenthesis is in the wrong place
  //    //if (strcmp(cmdType, "5" == 0)){ 
  //    //// The correct statment is:
  //    if (strcmp(cmdTarget, "5") == 0){
  //      if ((char)payload[0] == '1') {
  //        sprint(2, "KAKA", payload[0]);
  //        setLED(LED, LED_ON);
  //      } else {
  //        setLED(LED, LED_OFF);
  //      }
  //    }
  //  }
} //end of callback






/*
 *   mqttMsgCheck()
 *   
 *   Verify topic length of the message received is within limits
 */
void mqttMsgCheck(char* topic, byte* payload, unsigned int payloadLength){  
  int topic_length = strlen(topic);
  if (topic_length > mqttMaxTopicLength){
    //This is limited by the controller's settings: mqttTopicMaxLength
    sprint(0, "topic length greater than topic_size: ", strlen(topic));
  }   
  /* verify payload length is within limit */
  if(payloadLength >= mqttMaxPayloadLength){
    //TODO: Alert mqtt broker that payload is being truncated!
    //This is limited by the controller's settings
    sprint(0, "Payload exceeds max length of: ", mqttMaxPayloadLength);
  }
}




/*
 * sendRestart()
 * 
 * Publishes the last restart code (normal/crash, etc)
 * Using mqttTopic and mqttPayload arrays defined globally
 */
void sendRestart(){
  strcpy(mqttTopic, "status/");
  strcat(mqttTopic, nodeId);
  /* get payload */
  sprintf(mqttPayload, "%s", ESP.getResetInfo().c_str());         
  /* publish mqtt message to active broker */
  sendMqttMsg(mqttTopic, mqttPayload);
}




////THIS MIGHT BE NEEDED FOR GRACEFUL DISCONNECTS
/// BY PASSING AN OPTIONAL "GOING OFFLINE" ARGUMENT
///boolean connect (clientID, [username, password], [willTopic, willQoS, willRetain, willMessage], [cleanSession])
/*
 * sendState()
 * 
 * send a connect/online message (payload = 1) on boot
 * (see state 0 of state machine of manageMqtt)
 * 
 * FUTURE USE: Add argument for offline envent to send a graceful disconnect message (payload=0)
 * this will update the status t offline without sending the last_will
 * See checkMqttBrokers() for last_will connect message
 */
void sendState(){
  /* publish the initial node state topic with the retained flag ('true')*/
  /* Set mqtt topic */
  strcpy(mqttTopic, "status/");
  strcat(mqttTopic, nodeId);
  strcat(mqttTopic, "/state/");
  /* Set mqtt payload */  
  strcpy(mqttPayload, "1"); /* node is online */
  /* publish state topic with retain = true */
  sendMqttMsg(mqttTopic, mqttPayload, true);
}





/////////////////////////////////////////////
//THIS FUNCTION NEEDS A CHECK TO PREVENT mqttPayload BUFFER OVERRUN 
// WE ARE CONCATENATING A NUMBER OF STRINGS THAT COULD EXCEED THE SIZE OF THE BUFFER
////////////////////////////////////////////////////////////
/* 
 * sendStatus()
 * 
 * sends the following fields as payload separated by ':'
 *
 *  fw ver
 *  ssid
 *  wanIp
 *  lanIp
 *  number of AP clients
 *  mqtt brokers status flags
 */
void sendStatus(){
  /* Format message payload */  
  /* Add firmware version */
  strcpy(mqttPayload, FW_VERSION);          
  strcat(mqttPayload, ":");
  
  /* add wifi ssid the node is connected to */          
  WiFi.SSID().toCharArray(tempBuffer, WiFi.SSID().length()+1); 
  strcat(mqttPayload, tempBuffer);
  strcat(mqttPayload, ":");
  
  /* add wifi rssi the node is connected to */
  //https://stackoverflow.com/a/8257728
  sprintf(tempBuffer, "%d", WiFi.RSSI());
  strcat(mqttPayload, tempBuffer);
  strcat(mqttPayload, ":");
    
  /* add node's WAN IP */
  strcat(mqttPayload, wanIp);   
  strcat(mqttPayload, ":");
  
  /* add node's LAN IP */          
  toStringIp(WiFi.localIP()).toCharArray(tempBuffer, toStringIp(WiFi.localIP()).length()+1); 
  strcat(mqttPayload, tempBuffer);          
  strcat(mqttPayload, ":");
   
  /* add the number of clients connected to the Node's Captive Portal */
  //https://stackoverflow.com/a/22429675
  sprintf(tempBuffer, "%d", WiFi.softAPgetStationNum());
  strcat(mqttPayload, tempBuffer); 
  strcat(mqttPayload, ":");
  
  /* add mqtt primary broker status flag */
  sprintf(tempBuffer, "%d", mqttBrokerUpA);
  strcat(mqttPayload, tempBuffer);          
  strcat(mqttPayload, ":");

   /* add mqtt backup broker status flag */
  sprintf(tempBuffer, "%d", mqttBrokerUpB);
  strcat(mqttPayload, tempBuffer);     
 
  /* format mqtt topic */
  strcpy(mqttTopic, "status/");
  strcat(mqttTopic, nodeId); 
  
  /* publish mqtt message to active broker */
  sendMqttMsg(mqttTopic, mqttPayload);
} /* end of send status */




/*
 *   sendMqttMsg()
 *   
 *   Publish the given topic and payload to active broker
 *   If retain = true, send to all online brokers
 *   https://pubsubclient.knolleary.net/api#publish
 *   boolean publish (topic, payload, [length], [retained])
 */
void sendMqttMsg(char * mqttTopic, char * mqttPayload, bool retain){
  if (mqttBrokerUpA){ /* Connected to primary mqtt broker */
    sprint(2, "SENDING TO MQTT BROKER - PRIMARY",);     
    mqttClientA.publish(mqttTopic, mqttPayload, retain);
    if (retain && mqttBrokerUpB){ /* publish retained topics also to backup broker if up */
      sprint(2, "SENDING TO MQTT BROKER - BACKUP",);  
      mqttClientB.publish(mqttTopic, mqttPayload, retain);
    }
  }
  else{ /* use backup broker */
    sprint(2, "SENDING TO MQTT BROKER - BACKUP",);    
    mqttClientB.publish(mqttTopic, mqttPayload, retain);
  }
  sprint(2, mqttTopic, mqttPayload); 
  yield();
}


  


/*
 * configSettings() ////////RENAME TO getSettings() ???? /////////////////////////
 * 
 * Publish all configuration settings if none given
 * or Publish given setting.
 * If the current flash and ram values differ, send both
 * If a value is provided, change it in ram to validate
 * Use a separate function to save to FLASH and validate(wifi and broker), once all settings have been updated in ram.
 */
void configSettings(char * setting, char * value){
  bool fieldFound = 0;
  bool allFields = (strcmp(setting, "all")== 0) ? 1 : 0; /* set the flag if all fields are being requested */
  bool payload = (strlen(value) > 0) ? 1 : 0; /* set flag if there is a payload -- value to change the setting */
  bool ret = 0;
  int i;
  int fieldIndex;
  /* read values from flash into TEMP/FLASH struct */ 
  getCfgSettings(&cfgSettingsTemp);
  /* publish requested settings and update setting in ram if a value is given */  
  if (!allFields){
    /* check if the setting is valid */
    fieldIndex = getFieldIndex(setting);
    if (fieldIndex == ERROR){
      sprint(1, "Unknown Setting: ", setting);
      publishError(setting);
      return; 
    }
    if (payload){
       /* update given setting in RAM*/
       setSetting(fieldIndex, value);
       /* Publish current value(s) */
       publishIndex(fieldIndex);
    }else{
      /* No payload. just publish the current value(s) ram/flash of the given setting */
      publishIndex(fieldIndex);
    }
  }else{
  /* Publish all settings' values */     
    for (i=0; i<numberOfFields; i++){
      publishIndex(i);
      yield();
    }
  }
}



/*
 * publishIndex()
 * 
 * Publishes the ram and flash setting values for the given index
 * Requires that both flash and ram structs have been updated with eeprom/getCfgSettings()
 * 
 * This function is called by configSettings()
 */
void publishIndex(int index){
  /* check if given index is valid */
  if(index < 0 || index >= numberOfFields){
    sprint(1, "Invalid Struct Field Index",);
    publishError();
    return;
  }
  /* Valid index. Check if the value in flash is the same as value in active config in ram */
  unsavedChanges = false; 
  if (strcmp(structRamBase+field[index].offset, structFlashBase+field[index].offset) == 0){
      /* place value of current field to payload */
      strcpy(mqttPayload, structRamBase+field[index].offset);
  }else{
      /* ram and flash settings are now different. Set the flag for unsaved changes */
      unsavedChanges = true; /* this flag is reset by eeprom/saveAll() */
      /* Publish both values */      
      strcpy(mqttPayload, "CHANGE -----> :");  /* flag to highlight the log */        
      strcat(mqttPayload, structRamBase+field[index].offset);
      strcat(mqttPayload, ":");
      strcat(mqttPayload, structFlashBase+field[index].offset);      
  }
  /* append the setting name to the mqtt topic */
  strcpy(mqttTopic, "settings/"); 
  strcat(mqttTopic, nodeId); 
  strcat(mqttTopic, "/"); 
  strcat(mqttTopic, field[index].name); 
  /* publish mqtt message to active broker */
  sendMqttMsg(mqttTopic, mqttPayload);
}


/*
 * publishError()
 * 
 * Publishes an error topic like this:
 * **settings/F4CFA271B033/ERROR Unknown Setting
 * 
 * If the optional argument is included it will include that argument to the topic:
 * **settings/F4CFA271B033/kaka Unknown Setting
 */
void publishError(char *setting){
  /* append the setting name to the mqtt topic */
  strcpy(mqttTopic, "settings/"); 
  strcat(mqttTopic, nodeId); 
  strcat(mqttTopic, "/"); 
  strcat(mqttTopic, setting); 
  /* place error message into payload */
  strcpy(mqttPayload, "Unknown Setting"); 
  /* publish mqtt message to active broker */
  sendMqttMsg(mqttTopic, mqttPayload);
}

/*
 *   setSetting()
 *   
 *   Place the given setting value in RAM
 */
void setSetting(int index, char* value){
  bool ret = 0;
  ret = stringCopy(structRamBase+field[index].offset, value, field[index].size);
  if (ret){
    sprint(1, "INVALID VALUE FOR: ", field[index].name);
    sprint(1, "Value: ", value);  
  }else{
    sprint(1, "STORED VALUE: ", structRamBase+field[index].offset);  
  }
}


/*
 * getFieldIndex()
 * 
 * Locate the given setting in the field[] array
 * and return the field index or -1 if not found.
 */
int getFieldIndex(char * confSetting){
  bool found = 0;
  int i;
  for (i=0; i< numberOfFields; i++){
    /* check if value in flash is the same as value in active config in ram */
    if (strcmp(field[i].name, confSetting) == 0){
      found = 1;
      sprint(1, "Setting Found: ", field[i].name);       
      break;
    }
  }
  /* return field index or -1 if not found */
  return (found) ? i : ERROR;
}
