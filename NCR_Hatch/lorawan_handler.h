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
  DL_TX_INTERVAL_ID,
  DL_RESET_ID,
  DL_HOT_DC_ID,
  DL_PASSKEY_ID,
  DL_SAVE_SETTINGS_ID = 0x0A,
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


void loraRxTask(void * parameters); 

void loraTxTask(void * parameters); 

void lora_serial(const char* msg);
 
int initLora(uint8_t * appEUI, uint8_t * devEUI, uint8_t * appKEY, uint8_t * devADDR);

void initLoraSerial(void);

int switchClass(void);

int checkResponse (char * buffer, ERROR_MSGS errMsg);

/* Function pointers */
static void (*okCallback)(void) = NULL;
static void (*sendConfCallback)(void) = NULL;
static void (*saveAppeuiCallback)(uint8_t * appEUI) = NULL;
static void (*saveDeveuiCallback)(uint8_t * devEUI) = NULL;
static void (*saveAppkeyCallback)(uint8_t * appKey) = NULL;
static void (*saveTxIntervalCallback)(uint16_t txInterval) = NULL; 
static void (*saveHotDcCallback)(uint8_t hotDc) = NULL; 
static void (*saveSettingsCallback)(void) = NULL;
static void (*savePasskeyCallback)(uint8_t * passkey) = NULL;


// call back fuctions 
void processOK(void);
void processSendConfirmed(void);
void processDlAppeui(uint8_t * appEUI);
void processDlDeveui(uint8_t * devEUI);
void processDLAppkey(uint8_t * appKey);
void processDLTxinterval(uint16_t txInterval);
void processDLHotDc(uint16_t hotDc);
void processDLPasskey(uint8_t * passkey);
void processSaveSettings(void);


/* Register callback  */
void registerSendConfirmedCallback(void (*callback)(void));
void registerSaveAppeuiCallback(void (*callback)(uint8_t *));
void registerSaveDeveuiCallback(void (*callback)(uint8_t *));
void registerSaveAppkeyCallback(void (*callback)(uint8_t *));
void registerSaveTxIntervalCallback(void (*callback)(uint16_t));
void registerSaveHotDcCallback(void (*callback)(uint8_t));
void registerSaveSettingsCallback(void (*callback)(void));
void registerPasskeyCallback(void (*callback)(uint8_t *));


#endif