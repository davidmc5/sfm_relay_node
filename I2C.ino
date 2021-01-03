
//THIS FUCTION NEEDS SOME CHECKS TO VERIFY THE I2C DEVICES ARE WORKING
//NEED ABILITY TO DISCOVER AND SPECIFY ADITIONAL I2C DEVICES

void i2cInit(uint16_t* relayState) {
    //Wire.begin(int sda, int scl)
    //Wire.begin(2,14);
    Wire.begin(SDA, SCL);
    
  /* For testing - find all I2C device addresses */
    byte count = 0; // counts the number of I2C devices found
  for (byte i = 8; i < 120; i++){
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0){
      sprint(2, "Found I2C Device (decimal): ", i);
      count++;
      delay(1);
    }
  }
  if (count == 0){
  sprint(0, "No I2C Devices Found",);
}

  /* set 16-bit mode (default IOCON.BANK=0) and disable sequential address pointer (IOCON.SEQ=1) */
//  Wire.beginTransmission(0x20);
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(0x0A); //IOCON Configuration register (on default BANK=0)
  Wire.write(0x20); //Disable sequential address pointer
  Wire.endTransmission();
 
  /* set GPIO pins as outputs */
//  Wire.beginTransmission(0x20);
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(0x00); //IODIRA Direction register 
  Wire.write(0x00); //set all 8 A-ports as outputs
  Wire.write(0x00); //set all 8 B-ports as outputs
  Wire.endTransmission();

  //set all relays to off (default)
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(0x12); //GPIOA register 
  Wire.write(0xFF); //clear groupA
  Wire.write(0xFF); //clear groupB
  Wire.endTransmission();
}

///////utility function to set a bit of the relayState register
void setBit(uint16_t* relays, char* outBit, int val){

/////////////////////
  //if outBit == ALL, then set all bits
////////////////////
  
  //convert outBit to integer
  int8_t outBit_int = strtol(outBit, (char **)NULL, 10);
  
  //convert to range 0-15 (from given 1-16)
  outBit_int--;  

  //for testing
//  sprint(2,"Register Bit: ", outBit_int );
//  sprint(2,"VAL: ", val);

  if(val == 1){ 
    //set output to 0 (=ON -- open colector drivers)
    //sprint(2,"outBit: ", outBit_int );
  
    //https://stackoverflow.com/a/14467260
    //https://stackoverflow.com/a/7021750

    *relays &= ~(1UL << outBit_int); 
    
  }else{
     //set output to 1 (=OFF -- open colector drivers)
    *relays |= 1UL << outBit_int;   
  }
}


/* Controls IO Extender outputs via I2C interface */
void setOutput(uint16_t* relayState, char* outBit, int val){
   //update the relay register with the requested change
   setBit(relayState, outBit, val); 

  //split a 16-bit relay register into two separate bytes A and B

  /* Return A and B 8-relay groups for the MCP23018 */
  //https://stackoverflow.com/questions/53367838/how-to-split-16-value-into-two-8-bit-values-in-c
  uint8_t groupA = (uint8_t)(*relayState & 0x00FF); //get the two LSB
  uint8_t groupB = (uint8_t)((*relayState >> 8) & 0x00FF);

    /* Return A and B 8-relay groups for the MCP23017 */
//  uint8_t groupB = (uint8_t)(*relayState & 0x00FF); //get the two LSB
//  uint8_t groupA = (uint8_t)((*relayState >> 8) & 0x00FF);

  //FOR TESTING: TURNS ALL ON
//  groupA = 0x00;
//  groupB = 0x00;
  
  //Update relay latches
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(0x12); //GPIOA register 
  Wire.write(groupA); 
  Wire.write(groupB);
  Wire.endTransmission();
}
