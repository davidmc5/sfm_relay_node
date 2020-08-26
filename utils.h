
#ifdef DEBUG
  #define sprint(level, item, value) \
    if (level <= DEBUG && \
      level >= 0 && \
      level < sizeof(debugLevel)) \
    { \
      Serial.print(debugLevel[level]); \
      Serial.print(" --> ");  \
      Serial.print(item);  \
      Serial.print(": "); \
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
