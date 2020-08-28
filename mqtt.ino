
void manageMqtt(){
  if (WiFi.status() == WL_CONNECTED){

    if (!mqttClient.connected()) { 
        /* the mqttServer and callback are set in the setup() funtion */        
        if (mqttClient.connect("relayNode", mqttUser, mqttPassword )) {
     
          sprint(2, "Connected to MQTT Broker", mqttServer);

          /* Subscribe to any topics for my nodeId */
          strcpy(mqttTopic, nodeId);
          strcat(mqttTopic, "/#");
          mqttClient.subscribe(mqttTopic);

          //mqttClient.subscribe("#"); //subscribe to ALL FOR TESTING ONLY!
          //mqttClient.subscribe("led");
          //mqttClient.subscribe("relay/#");

          /* Publish hello message on power up */
          strcpy(mqttTopic, "hello/");
          strcat(mqttTopic, nodeId);          
          mqttClient.publish(mqttTopic, wanIp); //provide wanIp
          sprint(2, "MQTT Outgoing - Topic", mqttTopic);  
          sprint(2, "MQTT Payload", wanIp);


          /* Publish status message on power up */

          int stringLength = ESP.getResetInfo().length();

          /* make sure crach string fits in the array or truncate to max length */

          int length = (stringLength <= topic_max_length) ? stringLength : topic_max_length;
          ESP.getResetInfo().toCharArray(crashInfo, length+1);
          
          /*build the mqtt message */
          strcpy(mqttTopic, "status/");
          strcat(mqttTopic, nodeId);          
          mqttClient.publish(mqttTopic, crashInfo); //send crash info
          sprint(2, "MQTT Outgoing - Topic", mqttTopic);  
          sprint(2, "MQTT Payload", crashInfo);

         } else {   
          sprint(0, "Failed to Connect to MQTT - State", mqttClient.state());
          delay(5000);   
        }
    }
    mqttClient.loop();
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
//  sprint(2, "PAYLOAD VARIABLE LENGTH", length);

  //Declare message payload array for character string
  //////char msg[length+1]; //add space for the null termination -- WHY THIS WORKS W/O MALLOC?
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


  // USE A CASE SWITCH HERE FOR THE DIFFERENT TARGETS: RELAYS,SENSORS CONFIGURATION
   
  /* set relay on/off */ /////////////////Add this to the setOutput function!!!
  ////////////////////////////////////////////////////////////////////////////
  /////////////This should ONLY be 0 or 1 to set relays. 
  /////////////Capture other options to be handled differntly
  int relayAction = (payload[0]-'0'); //Convert to integer. 0=off, 1=on, 2=toggle, 3=pulse (on for 1 sec then off), (add an option for flashing)
  if(strcmp(topics[1], "relay") == 0){    
    setOutput(&relayState, topics[2], relayAction); //topics[2] is the relay#
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

  
