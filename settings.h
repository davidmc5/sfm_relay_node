/* 
 *  Configuration Settings
 *  Max flash: 512 bytes
 *    
 *  ITEMS TO ADD:
 *  SITE ADDRESS TO GET PUBLIC IP
 *  ERROR COUNTERS
 *  
 *  Previous settings (Delete this memo after upgrading to 1.22):
 *  
 *  struct cfgSettings_s{  
 *    char ap_ssid[32];
 *    char ap_pswd[32];
 *    char ap_ok[3];
 */
#define NUM_ELEMS(x)  (sizeof(x) / sizeof((x)[0]))
#define cfgStartAddr 0

struct cfgSettings_s{  
  char firstRun[3];       /* offset= 0 - Flag to detect very first boot of a new device: unitialized flash memory has garbage */
  char apSSIDlast[20];       /* offset= 3 */
  char apPSWDlast[20];       /* offset= 23 */
  
  char apSSIDa[20];       /* offset= 43 */
  char apPSWDa[20];       /* offset= 63 */
  
  char apSSIDb[20];       /* offset= 83 */
  char apPSWDb[20];       /* offset= 103 */
  
  char site_id[10];       /* offset= 123 - Populated by initial setup or HELLO handshake */
  char debug[5];          /* offset= 133 - s=serial, m=mqtt: example s0m2 = send 'alerts' to serial and 'all' to mqtt */
  char node_type[20];     /* offset= 138 - node type:  r-16;s-16 */
  char mqttServerA[20];   /* offset= 158 */
  char mqttPortA[6];      /* offset= 178 */
  char mqttUserA[20];     /* offset= 184 */
  char mqttPasswordA[20]; /* offset= 204 */
  char mqttServerB[20];   /* offset= 224 */
  char mqttPortB[6];      /* offset= 244 */
  char mqttUserB[20];     /* offset= 250 */
  char mqttPasswordB[20]; /* offset= 270 */
};

//struct cfgSettings_s{  
//  char firstRun[3];       /* offset= 0 - Flag to detect very first boot of a new device: unitialized flash memory has garbage */
//  char apSSIDa[20];       /* offset= 3 */
//  char apPSWDa[20];       /* offset= 23 */
//  char site_id[10];       /* offset= 43 - Populated by initial setup or HELLO handshake */
//  char debug[5];          /* offset= 53 - s=serial, m=mqtt: example s0m2 = send 'alerts' to serial and 'all' to mqtt */
//  char node_type[20];     /* offset= 58 - node type:  r-16;s-16 */
//  char mqttServerA[20];   /* offset= 78 */
//  char mqttPortA[6];      /* offset= 98 */
//  char mqttUserA[20];     /* offset= 104 */
//  char mqttPasswordA[20]; /* offset= 124 */
//  char mqttServerB[20];   /* offset= 144 */
//  char mqttPortB[6];      /* offset= 164 */
//  char mqttUserB[20];     /* offset= 170 */
//  char mqttPasswordB[20]; /* offset= 190 */
//};

/////////// CHANGE TO Settings_s, settingsRam and settingsFlash
///////////////////////////////////////////////////
struct cfgSettings_s cfgSettings={0}, cfgSettingsTemp={0}; //initialize all struct members to zero

/* set pointers for both flash and ram structs */
struct cfgSettings_s   * const settingsRamPtr = &cfgSettings; // set the struct pointer to the address of the Ram struct
struct cfgSettings_s   * const settingsFlashPtr = &cfgSettingsTemp; // set the struct pointer to the address of the FLASH struct
char *structRamBase = (char *)settingsRamPtr; //cast the base ram as a pointer to char 
char *structFlashBase = (char *)settingsFlashPtr; //cast the base flash as a pointer to char



/*
 * The following array of structs is used to obtain a field's name, offset and size
 * to detect and prevent buffer overruns
 * 
 * THIS COULD BE SIMPLIED BY USING X_MACROS
 * https://stackoverflow.com/a/38427990
 * http://www.c4learn.com/c-programming/c-initializing-array-of-structure/
 * 
 * ALL FIELD NAMES ARE LESS THAN 15 CHARACTERS. WE CAN SET char name[15] TO SAVE 90 BYTES
 */
struct Field_s{
  char name[20];
  int offset;
  int size;
};

/* 
 * Do not change the order of the setting below! 
 * their index is being used by the following functions:
 * 
 * mqtt/unsavedSettingsFound()
 */

 struct Field_s field[] = {
  {"firstRun", 0, 3},
  
  {"apSSIDlast", 3, 20},
  {"apPSWDlast", 23, 20},
  
  {"apSSIDa", 43, 20},
  {"apPSWDa", 63, 20},
  
  {"apSSIDb", 83, 20},
  {"apPSWDb", 103, 20},
  
  {"site_id", 123, 10},
  {"debug", 133, 5},
  {"node_type", 138, 20},
  {"mqttServerA", 158, 20},
  {"mqttPortA", 178, 6},
  {"mqttUserA", 184, 20},
  {"mqttPasswordA", 204, 20},
  {"mqttServerB", 224, 20},
  {"mqttPortB", 244, 6},
  {"mqttUserB", 250, 20},
  {"mqttPasswordB", 270, 20}
};

//struct Field_s field[] = {
//  {"firstRun", 0, 3},
//  {"apSSIDa", 3, 20},
//  {"apPSWDa", 23, 20},
//  {"site_id", 43, 10},
//  {"debug", 53, 5},
//  {"node_type", 58, 20},
//  {"mqttServerA", 78, 20},
//  {"mqttPortA", 98, 6},
//  {"mqttUserA", 104, 20},
//  {"mqttPasswordA", 124, 20},
//  {"mqttServerB", 144, 20},
//  {"mqttPortB", 164, 6},
//  {"mqttUserB", 170, 20},
//  {"mqttPasswordB", 190, 20}
//};


////////////////////////////////////
//// storage for the last 4 good wifi APs 
//struct wifiAP_s{
//  char apSsid[20];
//  char apPswd[20];
//};
//const int maxWifiAps = 4; /* maximum number of previous good wifi APs to store */
//struct wifiAP_s wifiAPs[maxWifiAps]{0}; /* LIFO stack for the most recent valid APs - Intialize all to null*/
//int lastWifiAp = 0; /* Store index to the current valid wifi AP */

///////////////////////////////////



const int numberOfFields = NUM_ELEMS(field);
/*
 * unsavedChanges
 * 
 * If true, ram and flash settings are different
 * Set by http/handleWifiSave and mqtt/unsavedSettingsFound()
 * Reset by eeprom saveAll() and getCfgSettings() 
 */
bool unsavedChanges = false; /* If true, ram and flash settings are different - Set by http/handleWifiSave and mqtt/unsavedSettingsFound(). Reset by eeprom saveAll() and getCfgSettings() */

bool wifiConfigChanges = false; /* save request if wifi settings are different between ram and flash - set by mqtt/unsavedSettingsFound() and http/handleWifiSave() */
bool mqttConfigChanges = false; /* save request if mqtt settings are different between ram and flash */
bool saveAllRequest = false; /* flag to indicate a save request has been made */
bool internetUp = false;
