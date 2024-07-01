#define DEBUG_P1
//#define DEBUG_SERIAL
// Update treshold in milliseconds, messages will only be sent on this interval
#define UPDATE_INTERVAL 5000000 // 5 second
//#define UPDATE_INTERVAL 10000 // 10 seconds
//#define UPDATE_INTERVAL 60000  // 1 minute
//#define UPDATE_INTERVAL 300000 // 5 minutes

long updateInterval = 5000000;  // 5 second

// Update treshold in milliseconds,
// this will also send values that are more than the tresholds time the same
#define UPDATE_FULL_INTERVAL 600000000 // 10 minutes
// #define UPDATE_FULL_INTERVAL 1800000 // 30 minutes
// #define UPDATE_FULL_INTERVAL 3600000 // 1 Hour

#define BAUD_RATE 115200
#define SERIAL_PIN_RX 5
#define P1_MAXLINELENGTH 1050

#define NUMBER_OF_READOUTS 7

long LAST_RECONNECT_ATTEMPT = 0;
long LAST_UPDATE_SENT = 0;
long LAST_FULL_UPDATE_SENT = 0;

char telegram[P1_MAXLINELENGTH];
uint8_t telegram_full[P1_MAXLINELENGTH];

/*struct TelegramDecodedObject
{
  String name;
  uint16_t attributeID;
  long value;
  long sendThreshold = 0;
  char code[16];
  char startChar = '(';
  char endChar = ')';
  bool sendData = false;
};*/

typedef struct {
    char name[50];  // Adjust the size as needed
    uint16_t attributeID;
    long value;
    long sendThreshold;
    char code[16];
    char startChar;
    char endChar;
    bool sendData;
} TelegramDecodedObject;

TelegramDecodedObject telegramObjects[NUMBER_OF_READOUTS];

unsigned int currentCRC = 0;