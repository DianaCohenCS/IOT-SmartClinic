#define USE_LITTLE false

#define FS_TYPE SPIFFS  // declare another type for testing, e.g. LittleFS
//#define FS_TYPE LittleFS // ERROR: 'LittleFS' was not declared in this scope
#define FS_FORMAT false   // You only need to format the filesystem once
#define MONITORING false  // set true to print for debugging