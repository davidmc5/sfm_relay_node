
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
 * FUTURE: if unable to connect to neither pri AND bak after a few retries,
 * try the two standby brokers (hardcoded)
 * 
 */

/* 
 *  manageMqtt()
 *  Implements and update a state machine to monitor brokers
 *  Publishes different mqtt status messages depending on broker failures
 *  If both brokers are down, try to reconnect to both
 *  After a few unsuccesful retries, try default brokers
 *  If only one broker is down, don't try to recover to avoid the few seconds delay
 *  Only send status changes once.
 */
void manageMqtt(){
  /* check mqtt conection status only if connected to wifi */
  if (WiFi.status() == WL_CONNECTED){
    mqttClientA.loop();
    mqttClientB.loop();
        
    switch(mqttBrokerState){       
      case 0: 
        //initial state after power up. Wait here until at least one broker is connected
        checkMqttBrokers();
       
        if (!mqttStatusA || !mqttStatusB){ //at least one broker is up
          mqttBrokerState = 1; //next state
          sendStatus();  /* Sending a status message will clear node error -> LED will stop blinking */
          sendRestart(); /*send node's restart code (crash message) only once after boot */ 
          mqttErrorCounter = 0;      
        }else{
          /* both brokers still down */
          mqttErrorCounter++;
          if (mqttErrorCounter > 0){
            loadMqttBrokerDefaults();          
            mqttErrorCounter = 0; 
          }
          sprint(1, "MQTT ERROR COUNTER >>>>>>>>>>>>>", mqttErrorCounter);        
        }        
        break;

      case 1: 
        //Normal state - Both brokers are up - Waiting for failures
        checkMqttBrokers(); 
        if (mqttStatusA && mqttStatusB){ //both brokers are down. Can't send messages!
          mqttBrokerState = 2; //next state
          blinker.attach(0.5, changeState); //blink LED
        }else if(mqttStatusA || mqttStatusB){ //only one broker down
          mqttBrokerState = 3;
          sendStatus();          
        }
        break;

      case 2: 
        //both brokers are down. Wait here until at least one recovers
        checkMqttBrokers();
        if (!mqttStatusA && !mqttStatusB){ //both brokers recovered
          mqttBrokerState = 1;
          mqttErrorCounter = 0;
          sendStatus();          
        }
        else if(mqttStatusA + mqttStatusB == 1){ //only one broker recovered
          mqttBrokerState = 3;
          mqttErrorCounter = 0;
          sendStatus();          
        }
        /* both brokers still down */
        mqttErrorCounter++;
        if (mqttErrorCounter > 0){
          loadMqttBrokerDefaults();
          mqttErrorCounter = 0;          
        }
        sprint(1, "MQTT ERROR COUNTER >>>>>>>>>>>>>", mqttErrorCounter);        
        break;
        
      case 3: 
        // one failure state. Wait here until full recovery or both brokers fail
        // Do not check connectivity here becasue it adds too much delay for the app
        // reconnection attempts block processing mqtt messages from the connected broker
        if (!mqttClientA.connected()) { mqttStatusA = 1;} 
        if (!mqttClientB.connected()) { mqttStatusB = 1;}         
        if (!mqttStatusA && !mqttStatusB){ //both brokers recovered
          mqttBrokerState = 1;
          sendStatus();          
        }else if (mqttStatusA && mqttStatusB){ //both brokers down
          mqttBrokerState = 2;
          blinker.attach(0.5, changeState); //blink LED
        }
        mqttErrorCounter = 0;
        break;        
    }
  } 
  else {
    /* not yet connected to wifi --wait a bit */
    delay(2000);
  }
} /* end of manageMqtt() */


/*
 * loadMqttBrokerDefaults()
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
}

/* 
 *  checkMqttBrokers() checks if node is connected to brokers and set status flags
 *  If node is not connected, attemp reconnect
 */
void checkMqttBrokers(){
  /* Test Primary mqtt broker */
  if (!mqttClientA.connected()) {
    mqttStatusA = 1; 
    /* the mqttServer and callback are set in the setup() function TASK 5 */ 
    //https://pubsubclient.knolleary.net/api#connect
    // boolean connect (clientID, [username, password], [willTopic, willQoS, willRetain, willMessage], [cleanSession])  
    /* mqtt username and password are defined in the file mqtt_brokers.h */

    mqttClientA.setServer(cfgSettings.mqttServerA, atoi(cfgSettings.mqttPortA));  //Primary mqtt broker
    mqttClientA.setCallback(mqttCallbackA); //function executed when a MQTT message is received.

    if (mqttClientA.connect(mqttClientId, cfgSettings.mqttUserA, cfgSettings.mqttPasswordA )) { 
      mqttStatusA = 0;  
      sprint(1, "Connected to MQTT Broker 1", cfgSettings.mqttServerA);
      //sprint(1, "MQTTCLIENTID", mqttClientId);
      /* Subscribe to all topics (/#) prefixed by this nodeId */
      strcpy(mqttTopic, nodeId);
      strcat(mqttTopic, "/#");
      mqttClientA.subscribe(mqttTopic);   
   } 
   else {   
      sprint(0, "Failure MQTT Broker 1 - State", mqttClientA.state());
   }
  } /*end of Primary mqtt broker test */
  /* Test Backup mqtt broker */
  if (!mqttClientB.connected()) { 
    mqttStatusB = 1;   
     
    mqttClientB.setServer(cfgSettings.mqttServerB, atoi(cfgSettings.mqttPortB)); //Backup mqtt broker
    mqttClientB.setCallback(mqttCallbackB); //function executed when a MQTT message is received. 
          
    if (mqttClientB.connect(mqttClientId, cfgSettings.mqttUserB, cfgSettings.mqttPasswordB)) { 
      mqttStatusB = 0;          
      sprint(1, "Connected to MQTT Broker 2", cfgSettings.mqttServerB);
      //sprint(1, "MQTTCLIENTID", mqttClientId);
      /* subscribe all topics for this nodeId (nodeId/#) */
      strcpy(mqttTopic, nodeId);
      strcat(mqttTopic, "/#");
      mqttClientB.subscribe(mqttTopic);
//      /* if any mqtt parameters different on flash, store them */      
//      saveSettings();
    }
    else {   
      sprint(0, "Failure MQTT Broker 2 - State", mqttClientB.state());
    }        
  } /* end of backup broker test */
  if (mqttStatusB + mqttStatusB > 0){
    /* Connected to any broker */  
    saveSettings(); /* Save mqtt parameters to flash, if different */ 
  }
} /* end of brokers test */



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
 *  Called when MQTT messages from either broker for this nodeid are received
 */
void mqttCallback(char* broker, char* topic, byte* payload, unsigned int length) {
  mqttMsgCheck(topic, payload, length); /* print a debub message if topic or payload strings are longer than buffers */
  /* copy the received bytes from payload to mqttPayload array */
  for (int i=0; i < length; i++){
    mqttPayload[i] = (char)payload[i];
  }
  mqttPayload[length]= NULL; /* add null termination for string */
  
  sprint(2, "MQTT BROKER", broker);  
  sprint(2, "MQTT Incomming - Topic", topic);  
  sprint(2, "MQTT Payload", mqttPayload);  
  /* 
   *  Parse received mqtt topic into an array of pointers
   *  
   *  Replace the delimiters '/' with null '/0' 
   *  and store the pointer of the start of each topic substring into  topics[] 
   *  Note that the given 'topic' buffer can't be written to replace the / with null 
   *  so we need to move the topic strig to the array rawTopics so we can replace the delimiters to form substrings
  */  
  
  /* Copy the topic string to a read/write array including the null string terminator*/
  /* Place each topic token (separated by '/') into the array of pointers topics[] */
  /* char *topics[maxTopics]will holds the pointers to each subtopic string */
  int topic_length = strlen(topic);
  /* rawTopic[mqttMaxTopicLength] will have the same string as the given 'topic' buffer but with '/' replaced with '/0' */
  char delim = '/';
  int numTopics = 1; /*number of topics (separated by '/'). Initially assume we have at least one */
  topics[0] = rawTopic; /* we have at least one topic's pointer */
  for (int i=0 ; i <= topic_length; i++){
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
  /*
   * ADD BELOW THE MQTT COMMANDS FOR THE NODE
   * ////////////////////////////////////////
   */   
  /* 
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
    sprint(2, "REQUESTED FIRMWARE UPGRADE", mqttPayload);
    t_httpUpdate_return ret = ESPhttpUpdate.update(mqttPayload);
    switch(ret) {
      case HTTP_UPDATE_FAILED:
        sprint(0, "HTTP_UPDATE_FAILED Error", ESPhttpUpdate.getLastErrorString().c_str());
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
    sprint(2, "REQUESTED STATUS UPDATE", mqttPayload);
    if ((mqttPayload[0]-'0') == 1){
      sprint(2, "Resetting MQTT Connections", payload[0]-'0');
      checkMqttBrokers();
    }
    sendStatus();
    sendConfig();
  }
  /*
   * SETTINGS
   * ////////
   * 
   * if no payload (query), send back the current setting
   * If there is a payload (set request), store the setting in flash and send confirmation
   */
   else if (strcmp(topics[1], "setting") == 0){
    if (strlen(mqttPayload)== 0){
      sprint(1, "topics[2]=", topics[2]);
    }else{ /* set setting to payload value*/
      sprint(1, topics[2], mqttPayload);
      setSetting(topics[2], mqttPayload);
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
 *   setSetting()
 *   store the given setting value in flash
 */
void setSetting(char* setting, char* value){
  for (int i=0; i< NUM_ELEMS(field); i++){
    if (strcmp(field[i].name, setting) == 0){
      sprint(1, "CURRENT SETTING VALUE", structRamBase+field[i].offset);      
      stringCopy(structRamBase+field[i].offset, value, field[i].size);
      EEPROM.begin(512);
      updateFlashField(structRamBase+field[i].offset, cfgStartAddr+field[i].offset, field[i].size);
      EEPROM.end();
      yield();
      sendStatus();
      break;     
    }
  }
}


/*
 *   mqttMsgCheck()
 *   Verify topic length of the message received is within limits
 */
void mqttMsgCheck(char* topic, byte* payload, unsigned int payloadLength){  
  int topic_length = strlen(topic);
  if (topic_length > mqttMaxTopicLength){
    //This is limited by the controller's settings: mqttTopicMaxLength
    sprint(0, "topic length greater than topic_size", strlen(topic));
  }   
  /* verify payload length is within limit */
  if(payloadLength >= mqttMaxPayloadLength){
    //TODO: Alert mqtt broker that payload is being truncated!
    //This is limited by the controller's settings
    sprint(0, "Payload exceeds max length of", mqttMaxPayloadLength);
  }
}

/*
 * sendRestart()
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


/////////////////////////////////////////////
//THIS FUNCTION NEEDS A CHECK TO PREVENT mqttPayload BUFFER OVERRUN 
// WE ARE CONCATENATING A NUMBER OF STRINGS THAT COULD EXCEED THE SIZE OF THE BUFFER
////////////////////////////////////////////////////////////



/* 
 * sendStatus()
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
  /* Since we have connectivity with a mqtt broker, turn LED on */
  blinker.detach();
  setLED(LED, LED_ON);

  /* Format message payload */
  
  /* add firmware version */
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
  sprintf(tempBuffer, "%d", mqttStatusA);
  strcat(mqttPayload, tempBuffer);          
  strcat(mqttPayload, ":");

   /* add mqtt backup broker status flag */
  sprintf(tempBuffer, "%d", mqttStatusB);
  strcat(mqttPayload, tempBuffer);     
 
  /* format mqtt topic */
  strcpy(mqttTopic, "status/");
  strcat(mqttTopic, nodeId); 
  
  /* publish mqtt message to active broker */
  sendMqttMsg(mqttTopic, mqttPayload);
}


/*
 * sendConfig()
 * Publish all the current configuration settings
 * Iterate through each of the config struct fields 
 * and publish a mqtt message for each
 * If the current flash and ram values differ, send both
 */
void sendConfig(){  
  /* read values from flash */ 
  EEPROM.begin(512);
  EEPROM.get(cfgStartAddr, cfgSettingsTemp);  

  /* set the mqtt topic */
  strcpy(mqttTopic, "status/"); //// WE MIGHT WANT TO CHANGE THE TOPIC TO 'SETTINGS' INSTEAD
  strcat(mqttTopic, nodeId); 

  /* iterate the struct fields and publish an mqtt message for each */
  for (int i=0; i< NUM_ELEMS(field); i++){
    if (strcmp(structRamBase+field[i].offset, structFlashBase+field[i].offset) == 0){
        sprint(1, field[i].name, structRamBase+field[i].offset);
        
        /* format payload */
        strcpy(mqttPayload, field[i].name);          
        strcat(mqttPayload, ":");
        strcat(mqttPayload, structRamBase+field[i].offset);
     }else{
        /* active config is different than flash. Send both */
        /* format payload */
        strcpy(mqttPayload, "*");  /* flag to highlight the log */        
        strcat(mqttPayload, field[i].name);          
        strcat(mqttPayload, ":");
        strcat(mqttPayload, structRamBase+field[i].offset);
        strcat(mqttPayload, ":");
        strcat(mqttPayload, structFlashBase+field[i].offset);      
     }
     /* publish mqtt message to active broker */
     sendMqttMsg(mqttTopic, mqttPayload);
     yield();
  }
  EEPROM.end();
}


/*
 *   sendMqttMsg()
 *   Publish given topic and payload to active broker
 */
void sendMqttMsg(char * mqttTopic, char * mqttPayload){
  if (!mqttStatusA){ /* primary mqtt broker is OK */
    sprint(2, "MQTT BROKER - PRIMARY",);     
    mqttClientA.publish(mqttTopic, mqttPayload);
  }
  else{ /* use backup broker */
    sprint(2, "MQTT BROKER - BACKUP",);     
    mqttClientB.publish(mqttTopic, mqttPayload);
  }
  sprint(2, "MQTT Outgoing - Topic", mqttTopic); 
  sprint(2, "MQTT Payload", mqttPayload);
}
  
