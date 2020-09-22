
void manageMqtt(){
  /* check mqtt conection status only if connected to wifi */
  if (WiFi.status() == WL_CONNECTED){

//    /* Test Primary mqtt broker */
//    if (!mqttClient1.connected()) {
//      mqttPriFlag = 1; 
//      /* the mqttServer and callback are set in the setup() function TASK 5 */ 
//      //https://pubsubclient.knolleary.net/api#connect
//      // boolean connect (clientID, [username, password], [willTopic, willQoS, willRetain, willMessage], [cleanSession])
//
//      /* mqtt username and password are defined in the file mqtt_brokeers.h */
//      if (mqttClient1.connect(mqttClientId, mqttUser1, mqttPassword1 )) { 
//        mqttPriFlag = 0;  
//        sprint(2, "Connected to MQTT Broker 1", mqttServer1);
//        /* set all topics (/#) for this nodeId */
//        strcpy(mqttTopic, nodeId);
//        strcat(mqttTopic, "/#");
//        mqttClient1.subscribe(mqttTopic);
//     } 
//     else {   
//        sprint(0, "Failure MQTT Broker 1 - State", mqttClient1.state());
////        delay(1000);   
//     }
//    }
    
    mqttClient1.loop();



    /* Test Backup mqtt broker */
//    if (!mqttClient2.connected()) { 
//      mqttBakFlag = 1; 
//             
//      if (mqttClient2.connect(mqttClientId, mqttUser2, mqttPassword2)) { 
//        mqttBakFlag = 0;          
//        sprint(2, "Connected to MQTT Broker 2", mqttServer2);
//        /* set all topics (/#) for this nodeId */
//        strcpy(mqttTopic, nodeId);
//        strcat(mqttTopic, "/#");
//        mqttClient2.subscribe(mqttTopic);
//      }
//      else {   
//        sprint(0, "Failure MQTT Broker 2 - State", mqttClient2.state());
////        delay(1000);   
//      }        
//    }

    mqttClient2.loop();



    /* state machine to send different messages depending on broker failures */
    switch(mqttBrokerState){ 
      
      case 0: 
        //initial state after power up. Wait until at least one broker is connected
        checkMqttPri();
        checkMqttBak();
        
        if (!mqttPriFlag || !mqttBakFlag){ //at least one broker is up
          mqttBrokerState = 1; //next state
          sendStatus();  /* Sending a status message will clear node error -> LED will stop blinking */
          sendRestart(); /*send node's restart code / crash message only after boot */
        }
        break;

      case 1: 
        //Normal state - Both brokers up - Waiting for failures
        checkMqttPri();
        checkMqttBak();
 
        if (mqttPriFlag && mqttBakFlag){ //both brokers are down. Can't send messages!
          mqttBrokerState = 2; //next state
          blinker.attach(0.5, changeState); //blink LED
        }
        else if(mqttPriFlag || mqttBakFlag){ //only one broker down
          mqttBrokerState = 3;
          sendStatus();          
        }
        break;

      case 2: 
        //both brokers are down. Wait here until at least one recovers
        checkMqttPri();
        checkMqttBak();

        if (!mqttPriFlag && !mqttBakFlag){ //both brokers recovered
          mqttBrokerState = 1;
          sendStatus();          
        }
        else if(mqttPriFlag + mqttBakFlag == 1){ //only one broker recovered
          mqttBrokerState = 3;
          sendStatus();          
        }
        break;
        
      case 3: 
        //one failure state. Wait here until full recovery or both brokers fail
        if (!mqttClient1.connected()) { mqttPriFlag = 1;} 
        if (!mqttClient2.connected()) { mqttBakFlag = 1;} 
        
        if (!mqttPriFlag && !mqttBakFlag){ //both brokers recovered
          mqttBrokerState = 1;
          sendStatus();          
        }
        else if (mqttPriFlag && mqttBakFlag){ //both brokers down
          mqttBrokerState = 2;
          blinker.attach(0.5, changeState); //blink LED
        }
        break;        
    }
  } 
  else {
    /* not yet connected to wifi --wait a bit */
    delay(2000);
  }
}

///////////////////////////////////////////////////
void checkMqttPri(){
    /* Test Primary mqtt broker */
    if (!mqttClient1.connected()) {
      mqttPriFlag = 1; 
      /* the mqttServer and callback are set in the setup() function TASK 5 */ 
      //https://pubsubclient.knolleary.net/api#connect
      // boolean connect (clientID, [username, password], [willTopic, willQoS, willRetain, willMessage], [cleanSession])

      /* mqtt username and password are defined in the file mqtt_brokeers.h */
      if (mqttClient1.connect(mqttClientId, mqttUser1, mqttPassword1 )) { 
        mqttPriFlag = 0;  
        sprint(2, "Connected to MQTT Broker 1", mqttServer1);
        /* set all topics (/#) for this nodeId */
        strcpy(mqttTopic, nodeId);
        strcat(mqttTopic, "/#");
        mqttClient1.subscribe(mqttTopic);
     } 
     else {   
        sprint(0, "Failure MQTT Broker 1 - State", mqttClient1.state());
//        delay(1000);   
     }
    }    
}

void checkMqttBak(){
    /* Test Backup mqtt broker */
    if (!mqttClient2.connected()) { 
      mqttBakFlag = 1; 
             
      if (mqttClient2.connect(mqttClientId, mqttUser2, mqttPassword2)) { 
        mqttBakFlag = 0;          
        sprint(2, "Connected to MQTT Broker 2", mqttServer2);
        /* set all topics (/#) for this nodeId */
        strcpy(mqttTopic, nodeId);
        strcat(mqttTopic, "/#");
        mqttClient2.subscribe(mqttTopic);
      }
      else {   
        sprint(0, "Failure MQTT Broker 2 - State", mqttClient2.state());
//        delay(1000);   
      }        
    }
}
/////////////////////////////////////////////////





///MQTT CALLBACKS
/* These functions are called when MQTT messages for this nodeid are received */


/* PRIMARY CALLBACK */
void mqttCallback(char* topic, byte* payload, unsigned int length) {



////////////////////
////// declare variables here instead... or return values from function
 mqttMsgCheck(topic, payload, length); /* print a debub message if topic or payload strings are longer than buffers */


  /* copy the received bytes from payload to mqttPayload array */
  char mqttPayload[length+1];
  for (int i=0; i < length; i++){
    mqttPayload[i] = (char)payload[i];
  }
  mqttPayload[length]= NULL; /* add null termination for string */
  sprint(2, "MQTT Incomming - Topic", topic);  
  sprint(2, "MQTT Payload", mqttPayload);

 
///////////////////////////////////////////////////////////////////////////
///// TO RESEARCH
///// THE FOLLOWING SEEMS TOTALLY WRONG, BUT APPEARS TO WORK! 
/////DECLARING AN ARRAY WITH A SIZE ONLY KNOWN AT RUN-TIME VARIABLE WITHOUT USING MEMORY ALLOCATION ?!
// "length" is not known until runtime!!!!!
//How does the compiler even allows this? 
//Answer: https://stackoverflow.com/a/737245
//char msg[length+1];
//see https://www.esp8266.com/viewtopic.php?f=28&t=2175
///////////////////////////////////////////////////////////////////////////

 
  /* 
   *  Parse received mqtt topic into an array of pointers
   *  
   *  Replace the delimiter '/' with a null '/0' 
   *  and store the pointer of the start of each topic substring into  topics[] 
   *  Note that the given 'topic' buffer can't be written to replace the / with null 
   *  so we need to declare a temporary array rawTopics that will be deallocated when the fuction exits
   *  We can call this function from both brokers since the variable scope is limited to each function call.
  */
  
  
  /* Copy the topic string to a read/write array including the null string terminator*/
  /* Place each topic token (separated by '/') into topics[] */
  char *topics[maxTopics]; /* this array holds the pointers to each topic token. */
  int topic_length = strlen(topic);
  char rawTopic[topic_max_length]; /* will have the same string as the given 'topic' buffer but with '/' replaced with '/0' */
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


      
   // USE A CASE SWITCH HERE FOR THE DIFFERENT TARGETS: RELAYS,SENSORS, CONFIGURATION, FWUPDATE
///////////////////////////////////////////////////////////////////////


  /* relay control */
  if(strcmp(topics[1], "relay") == 0){ 
    int relayAction = (payload[0]-'0'); //Convert to integer. 0=off, 1=on   
    setOutput(&relayState, topics[2], relayAction); //topics[2] is the relay#
  }

  /* firmware updates */
  else if (strcmp(topics[1], "fwupdate") == 0){
    /* msg is the mqtt payload received and must include the webserver address, port and filename */
    
    ////////////////////////
    sprint(2, "REQUESTED FIRMWARE UPGRADE", mqttPayload);
    t_httpUpdate_return ret = ESPhttpUpdate.update(mqttPayload);
    ///////////////////////////////////
    
    // t_httpUpdate_return ret = ESPhttpUpdate.update( "http://10.0.0.200:8000/fw_rev_2.bin");
    switch(ret) {
    case HTTP_UPDATE_FAILED:
      sprint(0, "HTTP_UPDATE_FAILED Error", ESPhttpUpdate.getLastErrorString().c_str());
      break;
    }
  }

  /* status update */
  else if (strcmp(topics[1], "status") == 0){
    //////////////////////////////////
    sprint(2, "REQUESTED STATUS UPDATE", mqttPayload);
    if ((payload[0]-'0') == 1){
      sprint(2, "Resetting MQTT Connections", payload[0]-'0');
      checkMqttPri();
      checkMqttBak();
    }

    //////////////////////////////////
    sendStatus();

    if ((payload[0]-'0') == 1){
      checkMqttPri();
      checkMqttBak();
    }
  }




/////////////////////////////////
//// TO RESEARCH
// THE CODE BELOW CAUSES A CRASH!
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



//void mqttCallback2(char* topic, byte* payload, unsigned int length) {
//
//  /* copy the received bytes from payload to mqttPayload array */
//  char mqttPayload[length+1];
//  for (int i=0; i < length; i++){
//    mqttPayload[i] = (char)payload[i];
//  }
//  mqttPayload[length]= NULL; /* add null termination for string */
//  sprint(2, "MQTT Incomming - Topic", topic);  
//  sprint(2, "MQTT Payload", mqttPayload);
//
//  
//  /* Copy the topic string to a read/write array including the null string terminator*/
//  /* Place each topic token (separated by '/') into topics[] */
//  char *topics[maxTopics]; /* this array holds the pointers to each topic token. */
//  int topic_length = strlen(topic);
//  char rawTopic[topic_max_length]; /* will have the same string as the given 'topic' buffer but with '/' replaced with '/0' */
//  char delim = '/';
//  int numTopics = 1; /*number of topics (separated by '/'). Initially assume we have at least one */
//
//  topics[0] = rawTopic; /* we have at least one topic's pointer */
//  for (int i=0 ; i <= topic_length; i++){
//    if (topic[i] == delim){
//      rawTopic[i] = NULL; //end of string delimiter
//      topics[numTopics++] = rawTopic+i+1; /* store the pointer to the next location after the delimeter */
//    }else if(topic[i] == NULL){
//      rawTopic[i] = NULL;
//      break; /* we are at the end of the topic string */
//    }else{
//      rawTopic[i] = topic[i];
//    }
//  }
// 
//  /* relay control */
//  if(strcmp(topics[1], "relay") == 0){ 
//    int relayAction = (payload[0]-'0'); //Convert to integer. 0=off, 1=on   
//    setOutput(&relayState, topics[2], relayAction); //topics[2] is the relay#
//  }
//
//  /* status update */
//  else if (strcmp(topics[1], "status") == 0){
//    //////////////////////////////////
//    sprint(2, "REQUESTED STATUS UPDATE", mqttPayload);
//    //////////////////////////////////
//    sendStatus();
//  }
//}


void mqttMsgCheck(char* topic, byte* payload, unsigned int payloadLength){
  /* Verify topic length of the message received is within limits */
  int topic_length = strlen(topic);
  if (topic_length > topic_max_length){
    //This is limited by the controller's settings:
    //mqttTopicMaxLength = 50
    sprint(0, "topic length greater than topic_size", strlen(topic));
  }
   
  /* verify payload length is within limit */
  if(payloadLength >= mqttMaxPayloadLength){
    //TODO: Alert mqtt broker that payload is being truncated!
    //This is limited by the controller's settings:
    //mqttMsgMaxLength = 50
    sprint(0, "Payload exceeds max length of", mqttMaxPayloadLength);
  }

//  /* copy the received bytes from payload to mqttPayload array */
//  for (int i=0; i < payloadLength; i++){
//    mqttPayload[i] = (char)payload[i];
//  }
//  mqttPayload[payloadLength]= NULL; /* add null termination for string */
//  sprint(2, "MQTT Incomming - Topic", topic);  
//  sprint(2, "MQTT Payload", mqttPayload);
}


void sendRestart(){
  /*get mqtt topic */
  strcpy(mqttTopic, "status/");
  strcat(mqttTopic, nodeId);
  /* get payload */

  /////////////////////////////////////////////////////////////
  sprintf(mqttPayload, "%s", ESP.getResetInfo().c_str());       
  /* publish mqtt message to active broker */
  sendMqttMsg(mqttTopic, mqttPayload);
}




void sendStatus(){
  /* 
   *  
   *send the following fields as payload
   * fw ver
   * ssid
   * wanIp
   * lanIp
   * number of AP clients
   * mqtt brokers status flags
   */

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
  sprintf(tempBuffer, "%d", mqttPriFlag);
  strcat(mqttPayload, tempBuffer);          
  strcat(mqttPayload, ":");

   /* add mqtt backup broker status flag */
  sprintf(tempBuffer, "%d", mqttBakFlag);
  strcat(mqttPayload, tempBuffer);     
 
  /* format mqtt topic */
  strcpy(mqttTopic, "status/");
  strcat(mqttTopic, nodeId); 
  
  /* publish mqtt message to active broker */
  sendMqttMsg(mqttTopic, mqttPayload);
}

void sendMqttMsg(char * mqttTopic, char * mqttPayload){
  /* publish function to active broker */
  if (!mqttPriFlag){ /* primary mqtt broker is OK */
    sprint(2, "MQTT BROKER - PRIMARY",);     
    mqttClient1.publish(mqttTopic, mqttPayload);
  }
  else{ /* use backup broker */
    sprint(2, "MQTT BROKER - BACKUP",);     
    mqttClient2.publish(mqttTopic, mqttPayload);
  }
  sprint(2, "MQTT Outgoing - Topic", mqttTopic); 
  sprint(2, "MQTT Payload", mqttPayload);
}
  
