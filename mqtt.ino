
void manageMqtt(){
  /* check mqtt conection status only if connected to wifi */
  if (WiFi.status() == WL_CONNECTED){

///////////////////////////////////////////////

      ///////////////////////
        //TEST BACKUP BROKER
        /////////////////////
    if (!mqttClient2.connected()) { 
      
        strcpy(clientId, "NODE-");
        strcat(clientId, nodeId);
        
        if (mqttClient2.connect(clientId, mqttUser, mqttPassword )) {
     
          sprint(2, "Connected to MQTT Broker 2!!!!!",);

          /* Subscribe to any topics for my nodeId */
          strcpy(mqttTopic, nodeId);
          strcat(mqttTopic, "/#");
          mqttClient2.subscribe(mqttTopic);
          delay(1000);
        }
        else {   
          sprint(0, "Failed to Connect to BROKER 2 - State", mqttClient2.state());
          delay(1000);   
        }        
    }
    mqttClient2.loop();

///////////////////////////////////////////////




    if (!mqttClient.connected()) { 
        /* the mqttServer and callback are set in the setup() function */ 
        //https://pubsubclient.knolleary.net/api#connect
        // boolean connect (clientID, [username, password], [willTopic, willQoS, willRetain, willMessage], [cleanSession])

        /* Get unique client ID to connect to mqtt broker - use NODE-MAC ADDr */
        strcpy(clientId, "NODE-");
        strcat(clientId, nodeId);

  
        //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
        /////////////////////////////////////////////////////////////////////////

        /* mqtt username and password are defined in the file mqtt_brokeers.h */
        if (mqttClient.connect(clientId, mqttUser, mqttPassword )) {
   
          sprint(2, "Connected to MQTT Broker 1", mqttServer);

          /* Subscribe to any topics for my nodeId */
          strcpy(mqttTopic, nodeId);
          strcat(mqttTopic, "/#");
          
          ///////////////////////////////////////
          mqttClient.subscribe(mqttTopic);

          //////////////////////////////////////
          
          //mqttClient.subscribe("#"); //subscribe to ALL FOR TESTING ONLY!
  

          /* Publish hello message on power up */
          strcpy(mqttTopic, "hello/");
          strcat(mqttTopic, nodeId);          
          mqttClient.publish(mqttTopic, wanIp); //provide wanIp
          sprint(2, "MQTT Outgoing - Topic", mqttTopic);  
          sprint(2, "MQTT Payload", wanIp);


          sendStatus();


          ///////////////////////////////////////////////////////////////////////////
          ///////////////////////////////////////////////////////////////////////////
          //try sending the restart code directly from the function without the buffer
          ///////////////////////////////////////////////////////////////////////////
          ///////////////////////////////////////////////////////////////////////////
          
          /* send restart reason message */
          int stringLength = ESP.getResetInfo().length();
          /* make sure crach string fits in the array or truncate to max length */
          int length = (stringLength <= topic_max_length) ? stringLength : topic_max_length-1;
          ESP.getResetInfo().toCharArray(restartCode, length+1); 
          restartCode[length+2] = '\0'; /* add a null terminator for the string */          
          /*build the mqtt message */
          strcpy(mqttTopic, "status/");
          strcat(mqttTopic, nodeId);          
          mqttClient.publish(mqttTopic, restartCode); //send crash info
          sprint(2, "MQTT Outgoing - Topic", mqttTopic);  
          sprint(2, "MQTT Payload", restartCode);
          delay(3000);

       } else {   
          sprint(0, "Failed to Connect to MQTT - State", mqttClient.state());
          delay(5000);   
        }
    }
    mqttClient.loop();
  } else {
    /* not yet connected to wifi --wait a bit */
    delay(5000);
  }
}





/*This function is called when a MQTT message is received */
//////////GIVE length A MORE DESCRIPTIVE NAME: mqttPayloadLength
////////////////////////////////////////////////////////////////
void mqttCallback(char* topic, byte* payload, unsigned int length) {
 
  /* Verify topic length is within limit */
  int topic_length = strlen(topic);
  if (topic_length > topic_max_length){
    //This is limited by the controller's settings:
    //mqttTopicMaxLength = 50
    sprint(0, "topic length greater than topic_size", strlen(topic));
  }

   
  /* verify payload length is within limit */
  if(length >= mqttMaxPayloadLength){
    //TODO: Alert mqtt broker that payload is being truncated!
    //This is limited by the controller's settings:
    //mqttMsgMaxLength = 50
    sprint(0, "Payload exceeds max length of", mqttMaxPayloadLength);
  }

 

//  char rawTopic[topic_max_length];
//  char delim = '/';
//  int numTopics = 1; /*number of topics (separated by '/'). Assume we have at least one */
  

///////////////////////////////////////////////////////////////////////////
///// TO RESEARCH
///// THIS MAY BE TOTALLY WRONG, BUT APPEARS TO WORK! 
/////INITIALIZING AN ARRAY USING A RUN-TIME VARIABLE WITHOUT MEMORY ALLOCATION ?!
// "length" is not known until runtime!!!!!
//How does the compiler even allows this?
//char msg[length+1];
//see https://www.esp8266.com/viewtopic.php?f=28&t=2175
///////////////////////////////////////////////////////////////////////////

  /* copy the received bytes from payload to a null terminated character string */
  //Declare message payload array for character string
  ////////WHY THIS EVEN WORKS W/O MALLOC given the length is not known at compile time?
  //////char msg[length+1]; //add space for the null termination 
  char msg[mqttMaxPayloadLength]; //Payload bytes
  
  /* convert payload bytes to a string and add null terminator */
  for (int i=0; i < length; i++){
    msg[i] = (char)payload[i];
  }
  msg[length]= NULL; /* add null termination for string */
  

  sprint(2, "MQTT Incomming - Topic", topic);  
  sprint(2, "MQTT Payload", msg);
 
  /* Get Topic */
  /* Copy the topic string to a read/write array including the null string terminator*/
  /* Place each topic token (separated by '/') into topics[] */
  char rawTopic[topic_max_length];
  char delim = '/';
  int numTopics = 1; /*number of topics (separated by '/'). Assume we have at least one */

  topics[0] = rawTopic; /* we have at least one topic */
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


//  // For testing: print each topic tokens separately
//  for (int i=0; i < numTopics; i++){
//    sprint(2, "topic", topics[i]);
//  }

      
  /* set siteId */
  ////////////////////////////////////////////////////////////////////////
  //////TODO: 
  //if (topics[1] == "siteId") and payload not empty, STORE siteId
  //if (topics[1] == "siteId") and paylod is '?' reply with siteId
  
  /////////////////////////////////////////////////////////////////////////


  // USE A CASE SWITCH HERE FOR THE DIFFERENT TARGETS: RELAYS,SENSORS, CONFIGURATION, FWUPDATE
   
  /* relay control */
  if(strcmp(topics[1], "relay") == 0){ 
    int relayAction = (payload[0]-'0'); //Convert to integer. 0=off, 1=on   
    setOutput(&relayState, topics[2], relayAction); //topics[2] is the relay#
  }

  /* firmware updates */
  else if (strcmp(topics[1], "fwupdate") == 0){
    /* msg is the mqtt payload received and must include the webserver address, port and filename */
    sprint(2, "REQUESTED FIRMWARE UPGRADE", msg);
    t_httpUpdate_return ret = ESPhttpUpdate.update(msg);
    // t_httpUpdate_return ret = ESPhttpUpdate.update( "http://10.0.0.200:8000/fw_rev_2.bin");
    switch(ret) {
    case HTTP_UPDATE_FAILED:
      //Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n",  ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      sprint(0, "HTTP_UPDATE_FAILED Error", ESPhttpUpdate.getLastErrorString().c_str());
      break;
    }
  }

  /* status update */
  else if (strcmp(topics[1], "status") == 0){
    sprint(2, "REQUESTED STATUS UPDATE", msg);
    sendStatus();
  }




/////////////////////////////////
//// TO RESEARCH
//    /////////// FOR CRASH TEST 
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



void mqttCallback2(char* topic, byte* payload, unsigned int length) {

   /* copy the received bytes from payload to a null terminated character string */
  //Declare message payload array for character string
  char msg[mqttMaxPayloadLength]; //Payload bytes
  /* convert payload bytes to a string and add null terminator */
  for (int i=0; i < length; i++){
    msg[i] = (char)payload[i];
  }
  msg[length]= NULL; /* add null termination for string */

  sprint(2, "BROKER 2-TOPIC", topic);
  sprint(2, "BROKER 2-PAYLOAD", msg);
}




  
void sendStatus(){
            /* 
           *  
           *send the following fields as payload
           *fw ver
           *ssid
           *wanIp
           *lanIp
           *# AP clients
            */

          /* Firmware version */
          strcpy(mqttPayload, FW_VERSION);          
          strcat(mqttPayload, ":");

          /* wifi ssid the node is connected to */          
          WiFi.SSID().toCharArray(tempBuffer, WiFi.SSID().length()+1); 
          strcat(mqttPayload, tempBuffer);
          strcat(mqttPayload, ":");

          /* wifi rssi the node is connected to */
          //https://stackoverflow.com/a/8257728
          sprintf(tempBuffer, "%d", WiFi.RSSI());
          strcat(mqttPayload, tempBuffer);
          
          strcat(mqttPayload, ":");

          sprint(2, "RSSI", WiFi.RSSI());


          /* Node's WAN IP */
          strcat(mqttPayload, wanIp);   
          strcat(mqttPayload, ":");
          
          /*Node's LAN IP */          
          toStringIp(WiFi.localIP()).toCharArray(tempBuffer, toStringIp(WiFi.localIP()).length()+1); 
          strcat(mqttPayload, tempBuffer);          
          strcat(mqttPayload, ":");

          //sprint(2, "LENGTH1", strlen(mqttPayload));

          
          /* Number of clients connected to Node's Captive Portal */
          //https://stackoverflow.com/a/22429675
//          tempBuffer[0] = '0'+ WiFi.softAPgetStationNum(); // CASTING TO (char) IS NOT WORKING!!
//          tempBuffer[1] = '\0'; /* null terminator to form a valid string */
          sprintf(tempBuffer, "%d", WiFi.softAPgetStationNum());
          strcat(mqttPayload, tempBuffer); 
 
          /* Send STATUS data*/
          strcpy(mqttTopic, "status/");
          strcat(mqttTopic, nodeId);          
          mqttClient.publish(mqttTopic, mqttPayload);
          
          sprint(2, "MQTT Outgoing - Topic", mqttTopic); 
          sprint(2, "MQTT Payload", mqttPayload);
}
  
