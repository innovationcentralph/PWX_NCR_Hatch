#ifndef LORAWAN_HANDLER_H
#define LORAWAN_HANDLER_H

#define LORAWAN_BAUDRATE 9600
#define LORAWAN_RXD2 16
#define LORAWAN_TXD2 17
#define MAX_BUFFER_SIZE 150

#define DEVICE_CONFIG_PORT        5


typedef enum
{
  DL_APPEUI_ID = 1,
  DL_DEVEUI_ID,
  DL_APPKEY_ID,
  DL_TX_INTERVAL_ID,//
  DL_RESET_ID,
  DL_HOT_DC_ID,
  DL_HEARTBEAT_INTERVAL_ID, // 
  DL_DC_STATES_ID, //
  DL_HOT_TIMEOUT_ID,//
  DL_PASSKEY_ID, // 
  DL_SAVE_SETTINGS_ID,
  DL_KEY_UPDATE_ENABLE_ID = 0xA5,
  DL_KEY_UPDATE_DISABLE_ID = 0x5A, 
}DOWNLINK_IDS; 

typedef enum
{ 
  INVALID_CONNECTION = 1, 
  FAILED_WRITE_DEUI,
  FAILED_WRITE_APPEUI,
  FAILED_WRITE_APPKEY,
  FAILED_WRITE_APPSKEY,  
  FAILED_WRITE_NWKKEY,
  FAILED_WRITE_NWKSKEY,
  FAILED_WRITE_DADDR,
  FAILED_SAVE_SETTINGS, 
  FAILED_JOINING,
  FAILED_SWITCH_CLASS,
  FAILED_TX_UNCONFIRMED,
  FAILED_TX_CONFIRMED,
} ERROR_MSGS;

inline uint8_t appEUI[8]= {0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10}; 
inline uint8_t devEUI[8]= {0x10,0x10,0x10,0x10,0x01,0xF9,0x42,0x1B}; 
inline uint8_t appKEY[16]= {0x8A,0xD7,0x3D,0x3D,0x3B,0x47,0xD4,0x3E,0xF5,0xB2,0xF8,0x25,0x26,0xAA,0xFB,0x50}; 
inline uint8_t devADDR[4]= {0xE0, 0x24, 0x4F, 0x52};

void loraRxTask(void * parameters); 

void loraTxTask(void * parameters); 

void lora_serial(const char* msg);
 
int initLora(uint8_t * appEUI, uint8_t * devEUI, uint8_t * appKEY, uint8_t * devADDR);

void initLoraSerial(void);

int switchClass(void);

int checkLinkc(void);

int checkResponse (char * buffer, ERROR_MSGS errMsg);

bool getLoraJoinStatus(void);
void setLoraJoinStatus(bool stat);

/* Function pointers */
static void (*okCallback)(void) = NULL;
static void (*sendConfCallback)(void) = NULL;

// call back fuctions 
void processOK(void);
void processSendConfirmed(void);

/* Register callback  */
void registerSendConfirmedCallback(void (*callback)(void));
void registerOkCallback(void (*callback)(void));


#endif