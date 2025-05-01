
#include <string.h> 
#include <stdio.h> 
#include <stdbool.h> 
#include <Arduino.h> 
#include <HardwareSerial.h>
#include "lorawan.h"
#include "lorawan_handler.h"

/* Variables */
/* LoRaWAN Credentials*/

uint16_t txInterval = 60000; 

HardwareSerial lorawanSerial(2);


int err= 0, count = 0; 
hearbeatPayload_s * hbPayloadPtr; 
diagnosticPayload_s * diagnosticPayloadPtr; 
eventsPayload_s * eventPayloadPtr; 
alarmPayload_s * alarmPayloadPtr; 

/* Utility */
void stringToHex(char * strBuffer, uint8_t * hexBuffer )
{
    int i; 
    int len = strlen(strBuffer);
    for(i =0 ; i < len/2; i++)
    { 
        char byteStr[3] = {strBuffer[i*2], strBuffer[i*2 + 1], '\0'}; // Get two characters
        hexBuffer[i] = (uint8_t)strtol(byteStr, NULL, 16); // Convert to byte (hex to integer)
    }
}


void loraRxTask(void * parameters)
{ 
  while(1)
  {
    if (lorawanSerial.available()) {
      unsigned long startTime = millis(); 
      String reply;
      while ((millis() - startTime) < 100) { // timeout 
        reply = lorawanSerial.readStringUntil('\n');

        if (strstr(reply.c_str(), "+EVT:SEND_CONFIRMED") != NULL) { 
          Serial.println("ACK");
          /* Todo: Uncomment and complete Callback function Here*/
          processSendConfirmed();
        } else if (strstr(reply.c_str(), "+EVT:") != NULL) {
            int port, size;
            char payload[35];

            // Step 2: Try to parse the pattern "+EVT:<port>:<code>:<hex>"
            if (sscanf(reply.c_str(), "+EVT:%d:%x:%34s", &port, &size, payload) == 3) {
              char printBuffer[MAX_BUFFER_SIZE];
              int len = strlen(payload);
              char charPayload[len];
              strcpy(charPayload, payload); 
              uint8_t numData[len/2]; 
              stringToHex(charPayload, numData);

              if(port == 5)
              {
                switch(numData[0])
                { 
                  case DL_APPEUI_ID: {
                    uint8_t appEUI[8];
                    memcpy(appEUI, &numData[1], 8 * sizeof(uint8_t));
                    sprintf(printBuffer,"APPEUI=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
                    appEUI[0], appEUI[1], appEUI[2], appEUI[3], appEUI[4], appEUI[5], appEUI[6], appEUI[7] );
                    Serial.print(printBuffer);

                    processDlAppeui(appEUI);
                  break; 
                  }
                  case DL_DEVEUI_ID: {
                    uint8_t devEUI[8];
                    memcpy(devEUI, &numData[1], 8 * sizeof(uint8_t));
                    sprintf(printBuffer,"DEVEUI=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
                    devEUI[0], devEUI[1], devEUI[2], devEUI[3], devEUI[4], devEUI[5], devEUI[6], devEUI[7] );
                    Serial.print(printBuffer);

                    processDlDeveui(devEUI);
                  break; 
                  }
                  case DL_APPKEY_ID: 
                    uint8_t appKEY[16];
                    memcpy(appKEY, &numData[1], 16 * sizeof(uint8_t));
                    sprintf(printBuffer,"APPKEY=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
                    appKEY[0], appKEY[1], appKEY[2], appKEY[3], appKEY[4], appKEY[5], appKEY[6], appKEY[7],
                    appKEY[8], appKEY[9], appKEY[10], appKEY[11], appKEY[12], appKEY[13], appKEY[14], appKEY[15]);
                    Serial.print(printBuffer);

                    processDLAppkey(appKEY); 
                  break; 
                  case DL_TX_INTERVAL_ID: {
                    Serial.print("Tx Interval: ");
                    uint16_t txInt = (uint16_t)((numData[1] << 4) | numData[2]);
                    Serial.println(txInt);

                    processDLTxinterval(txInt);   
                  break;
                  } 
                  
                  case DL_RESET_ID: 
                    Serial.print("Reset");
                    lorawanSerial.print("ATZ\r");
                    delay(10);
                    // ESP.restart();

                  break; 

                  case DL_HOT_DC_ID: 
                    Serial.print("Hot DC: ");
                    Serial.println(numData[1]);
    
                    processDLHotDc(numData[1]); 
                  break; 

                  case DL_PASSKEY_ID: 
                    Serial.print("PASSKEY: ");
                    uint8_t passkey[6]; 
                    memcpy(passkey, &numData[1], 6 * sizeof(uint8_t));
                    sprintf(printBuffer,"Passkey = %02X-%02X-%02X-%02X-%02X-%02X\r\n", 
                    passkey[0], passkey[1], passkey[2], passkey[3], passkey[4], passkey[5] );
                    Serial.println(printBuffer);


                    processDLPasskey(passkey); 
                  break; 
                  
                  
                  case DL_KEY_UPDATE_ENABLE_ID: 
                    Serial.print("Update credentials enable");
                    /* Todo: add callback */ 
                  break; 

                  case DL_KEY_UPDATE_DISABLE_ID: 
                    Serial.print("Update credentials disable");
                    /* Todo: add callback */ 
                  break; 

                  case DL_SAVE_SETTINGS_ID: 
                    Serial.print("Saving Settings");
                    /* Todo: Uncomment and complete Callback function Here*/
                    processSaveSettings(); 
                  break; 
                  default: 
                  break; 
                }
              }
            }
        } else {
            // Serial.println("+EVT line not found.\n");
        }

        if (strstr(reply.c_str(), "OK") != NULL) { 
          Serial.println("OK");
          processOK();
        }
        #ifdef ECHO_RX_REPLY
        Serial.println(reply); // Echo the received reply
        #endif
      }

      
      
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); // Shorter delay for faster response
  }
}

void loraTxTask(void * parameters)
{ 
  hbPayloadPtr = heartbeatPayloadInstance();
  diagnosticPayloadPtr = getDiagnosticPayloadInstance();
  eventPayloadPtr = getEventsPayloadInstance();
  alarmPayloadPtr = getAlarmPayloadInstance();  
  TickType_t lastWakeTime = xTaskGetTickCount();
  while(1)
  {
    int err;
    if(count ==0)
    {
      hbPayloadPtr->temperature.all = 334;// 33.4 multiplied by 10 
      hbPayloadPtr->humidity.all = 501;// 33.4 multiplied by 10
      hbPayloadPtr->siren = 1;// 
      hbPayloadPtr->dryContactStat.dc1  = 1;
      hbPayloadPtr->dryContactStat.dc2  = 0;
      hbPayloadPtr->dryContactStat.dc3  = 1;
      hbPayloadPtr->dryContactStat.dc4  = 0;
      hbPayloadPtr->dryContactStat.dc5  = 1;
      hbPayloadPtr->dryContactStat.dc6  = 0;

      err = processUplink(HEARTBEAT,  UNCONFIRMED_UPLINK);
      
    }
    else if(count == 1)
    {
      diagnosticPayloadPtr->dcVolt.all = 261; // 10.6v multiplied by 10 
      diagnosticPayloadPtr->dcCurr.all = 53; // 51v multiplied by 10 
      diagnosticPayloadPtr->battVolt.all = 96; // 10.6v multiplied by 10 
      diagnosticPayloadPtr->battCurr.all = 31; // 51v multiplied by 10 
     

      err = processUplink(DIAGNOSTIC,  UNCONFIRMED_UPLINK);
    }
    else if(count == 2)
    {
      alarmPayloadPtr->dryContactStat.dc1  = 1;
      alarmPayloadPtr->dryContactStat.dc2  = 0;
      alarmPayloadPtr->dryContactStat.dc3  = 1;
      alarmPayloadPtr->dryContactStat.dc4  = 0;
      alarmPayloadPtr->dryContactStat.dc5  = 1;
      alarmPayloadPtr->dryContactStat.dc6  = 0;
      err = processUplink(ALARM_THEFT,  CONFIRMED_UPLINK);
    }
    else if(count == 3)
    {
      alarmPayloadPtr->dryContactStat.dc1  = 1;
      alarmPayloadPtr->dryContactStat.dc2  = 0;
      alarmPayloadPtr->dryContactStat.dc3  = 1;
      alarmPayloadPtr->dryContactStat.dc4  = 0;
      alarmPayloadPtr->dryContactStat.dc5  = 1;
      alarmPayloadPtr->dryContactStat.dc6  = 0;
      err = processUplink(SILENT_ALARM,  CONFIRMED_UPLINK);
    }
    else if(count == 4)
    {
      eventPayloadPtr->eventOccured = DC_CHANGE_OF_STATE; 
      eventPayloadPtr->temperature.all =  333; 
      eventPayloadPtr->humidity.all = 555; 
      eventPayloadPtr->dryContactStat.dc1  = 1;
      eventPayloadPtr->dryContactStat.dc2  = 0;
      eventPayloadPtr->dryContactStat.dc3  = 1;
      eventPayloadPtr->dryContactStat.dc4  = 0;
      eventPayloadPtr->dryContactStat.dc5  = 1;
      eventPayloadPtr->dryContactStat.dc6  = 0;
      err = processUplink(EVENTS,  CONFIRMED_UPLINK);
    }
    count = count + 1; 

    if (count > 4)
    {
      count =0; 
    }

    if(err) 
    {
      Serial.println("Sending Error");
    }
    else {
      Serial.println("Sending Done");
    }
    vTaskDelayUntil(&lastWakeTime, txInterval / portTICK_PERIOD_MS);
  }
}

void lora_serial(const char* msg) {
    lorawanSerial.println(msg); 
}

void initLoraSerial(void)
{


  /* Initialize Serial */
  lorawanSerial.begin(LORAWAN_BAUDRATE, SERIAL_8N1, LORAWAN_RXD2, LORAWAN_TXD2);
  /* Initialize Serial */
  lora_serial_init(lora_serial);

}
int initLora(uint8_t * appEUI, uint8_t * devEUI, uint8_t * appKEY, uint8_t * devADDR)
{ 
  int err; 
  char buffer[MAX_BUFFER_SIZE];

  delay(3000);
  // empty rx buffer.
  // unsigned long startTime = millis();
  // String reply;
  // while ((millis() - startTime) < 1000) { // timeout 
  //   reply = lorawanSerial.readString();
  // }

  /* Check connection with at SLAVE */
  lorawanSerial.print("AT\r\n");
  delay(100);
  err = checkResponse(buffer, INVALID_CONNECTION); 
  if(err) return err;  
  Serial.print("Connection is valid \n\r"); 

  memset(buffer, 0, sizeof(buffer)); 

  /* Set APPEUI */
  sprintf(buffer,"AT+APPEUI=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
  appEUI[0], appEUI[1], appEUI[2], appEUI[3], appEUI[4], appEUI[5], appEUI[6], appEUI[7] );
  
  lorawanSerial.print(buffer);
  
  delay(100);
  memset(buffer, 0, sizeof(buffer)); 
  err = checkResponse(buffer, FAILED_WRITE_APPEUI); 
  if(err) return err;  
  Serial.print("APPEUI is set\n\r"); 

  /* Set devEUI */
  sprintf(buffer,"AT+DEUI=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
  devEUI[0], devEUI[1], devEUI[2], devEUI[3], devEUI[4], devEUI[5], devEUI[6], devEUI[7] );

  lorawanSerial.print(buffer);
  delay(100);
  memset(buffer, 0, sizeof(buffer)); 
  err = checkResponse(buffer, FAILED_WRITE_DEUI); 
  if(err) return err;  
  Serial.print("DEUI is set\n\r"); 

  /* Set appKEY */
  sprintf(buffer,"AT+APPKEY=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
          appKEY[0], appKEY[1], appKEY[2], appKEY[3], appKEY[4], appKEY[5], appKEY[6], appKEY[7],
          appKEY[8], appKEY[9], appKEY[10], appKEY[11], appKEY[12], appKEY[13], appKEY[14], appKEY[15]);
  lorawanSerial.print(buffer);
  delay(100);
  memset(buffer, 0, sizeof(buffer)); 
  err = checkResponse(buffer, FAILED_WRITE_APPKEY); 
  if(err) return err;  
  Serial.print("APPKEY is set\n\r"); 

  /* Set nwkKEY */
  sprintf(buffer,"AT+NWKKEY=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
          appKEY[0], appKEY[1], appKEY[2], appKEY[3], appKEY[4], appKEY[5], appKEY[6], appKEY[7],
          appKEY[8], appKEY[9], appKEY[10], appKEY[11], appKEY[12], appKEY[13], appKEY[14], appKEY[15]);
  lorawanSerial.print(buffer);
  delay(100);
  memset(buffer, 0, sizeof(buffer)); 
  err = checkResponse(buffer, FAILED_WRITE_NWKKEY); 
  if(err) return err;  
  Serial.print("NWKKEY is set\n\r"); 

    /* Set appsKEY */
  sprintf(buffer,"AT+APPSKEY=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
          appKEY[0], appKEY[1], appKEY[2], appKEY[3], appKEY[4], appKEY[5], appKEY[6], appKEY[7],
          appKEY[8], appKEY[9], appKEY[10], appKEY[11], appKEY[12], appKEY[13], appKEY[14], appKEY[15]);
  lorawanSerial.print(buffer);
  delay(100);
  memset(buffer, 0, sizeof(buffer)); 
  err = checkResponse(buffer, FAILED_WRITE_APPSKEY); 
  if(err) return err;  
  Serial.print("APPSKEY is set\n\r"); 

  /* Set nwksKEY */
  sprintf(buffer,"AT+NWKSKEY=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
          appKEY[0], appKEY[1], appKEY[2], appKEY[3], appKEY[4], appKEY[5], appKEY[6], appKEY[7],
          appKEY[8], appKEY[9], appKEY[10], appKEY[11], appKEY[12], appKEY[13], appKEY[14], appKEY[15]);
  lorawanSerial.print(buffer);
  delay(100);
  memset(buffer, 0, sizeof(buffer)); 
  err = checkResponse(buffer, FAILED_WRITE_NWKSKEY); 
  if(err) return err;  
  Serial.print("NWKSKEY is set\n\r"); 

    /* Set devADDR */
  sprintf(buffer,"AT+DADDR=%02X:%02X:%02X:%02X\r\n", 
          devADDR[0], devADDR[1], devADDR[2], devADDR[3]);
  lorawanSerial.print(buffer);
  delay(100);
  memset(buffer, 0, sizeof(buffer)); 
  err = checkResponse(buffer, FAILED_WRITE_DADDR); 
  if(err) return err;  
  Serial.print("Dev ADDR is set\n\r"); 

  /* Set devADDR */

  lorawanSerial.print("AT+CS\r");
  delay(100);
  err = checkResponse(buffer, FAILED_SAVE_SETTINGS); 
  if(err) return err;  
  Serial.print("Configuration is saved successfully \n\r"); 

  delay(100);
  /* Set Joining */
  Serial.print("Joining."); 
  memset(buffer, 0, sizeof(buffer)); 
  bool isJoined = false; 

  while(!isJoined)
  {
    int i = 0; 
    unsigned long startTime; 
    startTime = millis();

    lorawanSerial.print("AT+JOIN=1\r");
    delay(10);
    String c;
    while ((millis() - startTime) < 10000) {
        while (lorawanSerial.available()) {
            c = lorawanSerial.readStringUntil('\n');
            if(strstr(c.c_str(), "+EVT:JOINED") != NULL)
            { 
              isJoined = true;
            }
             Serial.println(c); 
        }
        delay(10); 
    }
 
    delay(5000);
  }
  Serial.println("\n\rJOINED SUCCESSFULLY \n\r");
  return 0; 
  
}

int switchClass(void)
{
  delay(5000);
  char buffer[MAX_BUFFER_SIZE];
  int  err;  
  /* Switch Class */
  lorawanSerial.print("AT+CLASS=C\r");
  delay(10);
  err = checkResponse(buffer, FAILED_SWITCH_CLASS); 
  if(err) return err;  
  Serial.print("Switched to class C \n\r"); 


  /* Initial Transmission */
  lorawanSerial.print("AT+SEND=10:0:1234\r\n");
  delay(10);
  err = checkResponse(buffer, FAILED_TX_UNCONFIRMED); 
  if(err) return err;  
  Serial.print("Initial transmission done \n\r"); 

  delay(5000);
  return 0;
}

int checkResponse (char * buffer, ERROR_MSGS errMsg)
{ 
  int i = 0; 
  unsigned long startTime; 
  startTime = millis();
  String reply;
  while ((millis() - startTime) < 100) {
    while (lorawanSerial.available()) {
        reply = lorawanSerial.readStringUntil('\n');
    }
      if (strstr(reply.c_str(), "OK") != NULL)
    { 
        return 0; 
    }
    else
    { 
      Serial.print("[Err:"); 
      Serial.print(errMsg);
      Serial.print("]");
      Serial.print(buffer);

      return errMsg; 
    }
  }

  return 0;
 
}



void processOK(void) {
    if (okCallback) {
        okCallback();
    }
}
void processSendConfirmed(void) {
    if (sendConfCallback) {
        sendConfCallback();
    }
}
void processDlAppeui(uint8_t * appEUI) {
    if (saveAppeuiCallback) {
        saveAppeuiCallback(appEUI);
    }
}
void processDlDeveui(uint8_t * devEUI) {
    if (saveDeveuiCallback) {
        saveDeveuiCallback(devEUI);
    }
}
void processDLAppkey(uint8_t * appKey) {
    if (saveAppkeyCallback) {
        saveAppkeyCallback(appKey);
    }
}
void processDLTxinterval(uint16_t txInterval) {
    if (saveTxIntervalCallback) {
        saveTxIntervalCallback(txInterval);
    }
}
void processDLHotDc(uint16_t hotDc) {
    if (saveHotDcCallback) {
        saveHotDcCallback(hotDc);
    }
}

void processDLPasskey(uint8_t * passkey) {
    if (savePasskeyCallback) {
        savePasskeyCallback(passkey);
    }
}
void processSaveSettings(void) {
    if (saveSettingsCallback) {
        saveSettingsCallback();
    }
}



/* Register callback  */
void registerSendConfirmedCallback(void (*callback)(void)) {
    sendConfCallback = callback;
}
void registerSaveAppeuiCallback(void (*callback)(uint8_t *)) {
    saveAppeuiCallback = callback;
}
void registerSaveDeveuiCallback(void (*callback)(uint8_t *)) {
    saveDeveuiCallback = callback;
}
void registerSaveAppkeyCallback(void (*callback)(uint8_t *)) {
    saveAppkeyCallback = callback;
}
void registerSaveTxIntervalCallback(void (*callback)(uint16_t)) {
    saveTxIntervalCallback = callback;
}
void registerSaveHotDcCallback(void (*callback)(uint8_t)) {
    saveHotDcCallback = callback;
}
void registerSaveSettingsCallback(void (*callback)(void)) {
    saveSettingsCallback = callback;
}
void registerPasskeyCallback(void (*callback)(uint8_t *)) {
    savePasskeyCallback = callback;
}

