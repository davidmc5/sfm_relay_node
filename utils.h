/////////////////////////////
//MOVE THIS DEFINITION TO A HEADER FILE WITH THE REST OF THE EEPROM TOOLS
/////////////////////////////////////
#define GETCFG(M, P)\
  EEPROM.begin(512);  \
  /* Calculate the starting eeprom address of the given structure member M */ \
  /* cfgStartAddr is the starting address of the struct in the eeprom */ \
  /* offsetof(apStruct_t, M)) is the member offset from the start of the struct; */ \
  EEPROM.get(cfgStartAddr+offsetof(cfgSettings_s, M), P);   \
  delay(200);\
  EEPROM.end();\



///////////////////////

#ifdef DEBUG
  #define sprint(level, item, value) \
    if (level <= DEBUG && \
      level >= 0 && \
      level < sizeof(debugLevel)) \
    { \
      Serial.print(debugLevel[level]); \
      Serial.print(" --> ");  \
      Serial.print(item);  \
      Serial.println(value); \
    }
#else
    //define an empty macro so the compiler does not complain
    #define sprint(level,item, value) (void)0
#endif

void serialInit(){
  #ifdef DEBUG
    Serial.begin(BAUD);     // Initialize Serial for debug print.
    while (!Serial) {
      ; // Wait for serial port to connect.
    }
    Serial.println();
    Serial.println("---------------------");
    Serial.println("DEBUG CONSOLE STARTED");
    Serial.print("Level: ");
    Serial.println(DEBUG);
    Serial.println("---------------------");
  #endif
}
