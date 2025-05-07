
#include <string.h> 
#include <stdio.h> 
#include <stdbool.h> 
#include <Arduino.h> 
#include <HardwareSerial.h>
#include "lorawan.h"
#include "lorawan_handler.h"
#include "CLITask.h"

#define NEXT_CHECK_LINK_AFTER 200
#define NUM_OF_LINK_CHECK 3
/* Variables */
/* LoRaWAN Credentials*/

uint16_t txInterval = 60000; 

HardwareSerial lorawanSerial(2);




int err= 0, count = 0; 
hearbeatPayload_s * hbPayloadPtr; 
diagnosticPayload_s * diagnosticPayloadPtr; 
eventsPayload_s * eventPayloadPtr; 
alarmPayload_s * alarmPayloadPtr; 
keysPayload_s * keysPayloadPtr;
volatile bool loraJoinStatus = false; 

/**/
bool isCheckingLink=false; 
uint8_t nextCheckLinkCounter=0; 
uint8_t successfulLinkCheck=0; 
uint8_t sentCheckLinkReq=0; 
uint8_t linkCheckDelayCounter=0; 
bool rejoin = false; 
bool getLoraJoinStatus(void)
{
  bool status =  loraJoinStatus; 
  return status;
}

void setLoraJoinStatus(bool stat)
{
  loraJoinStatus = stat;
}


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
    if(getLoraJoinStatus())
    {
      if (lorawanSerial.available()) {
        unsigned long startTime = millis(); 
        String reply;
        
        while ((millis() - startTime) < 100) { // timeout 
          reply = lorawanSerial.readStringUntil('\n');
          // Serial.println(reply);
          if (strstr(reply.c_str(), "AT_NO_NET_JOINED") != NULL) { 
            Serial.println("============= NOT Connected to LoRaWAN Network =============");
            /* Todo: Uncomment and complete Callback function Here*/
            
          }
          if (strstr(reply.c_str(), "+EVT:SEND_CONFIRMED") != NULL) { 
            Serial.println("ACK");

            if(isCheckingLink)
            {
              successfulLinkCheck++;
            }
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
                    }
                    break; 
                    
                    case DL_DEVEUI_ID: {
                      uint8_t devEUI[8];
                      memcpy(devEUI, &numData[1], 8 * sizeof(uint8_t));
                      sprintf(printBuffer,"DEVEUI=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
                      devEUI[0], devEUI[1], devEUI[2], devEUI[3], devEUI[4], devEUI[5], devEUI[6], devEUI[7] );
                      Serial.print(printBuffer);
                    }
                    break; 
                    
                    case DL_APPKEY_ID: 
                      uint8_t appKEY[16];
                      memcpy(appKEY, &numData[1], 16 * sizeof(uint8_t));
                      sprintf(printBuffer,"APPKEY=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
                      appKEY[0], appKEY[1], appKEY[2], appKEY[3], appKEY[4], appKEY[5], appKEY[6], appKEY[7],
                      appKEY[8], appKEY[9], appKEY[10], appKEY[11], appKEY[12], appKEY[13], appKEY[14], appKEY[15]);
                      Serial.print(printBuffer);

                    break; 
                    case DL_TX_INTERVAL_ID: {  
                      Serial.println("DL LoRa TX Interval");
                      String cmd = "AT+SET_LORA_INTERVAL=";
                      //  uint16_t txInt = (uint16_t)((numData[1] << 8) | numData[2]);
                      uint32_t txInt = ((uint32_t)((numData[1] << 8) | numData[2]))*1000;
                      Serial.print("Tx Interval: ");
                      cmd += (String)txInt; 
                      Serial.println(cmd);
                      handleCLICommand(cmd); 
                    } 
                    break;
                  
                    
                    case DL_RESET_ID: 
                      Serial.print("Reset");
                      lorawanSerial.print("ATZ\r");
                      vTaskDelay(pdMS_TO_TICKS(10));
                      ESP.restart();

                    break; 

                    case DL_HOT_DC_ID: 
                      Serial.print("Hot DC: ");
                      Serial.println(numData[1]);
      
                    break; 
                    case DL_HEARTBEAT_INTERVAL_ID: {
                      Serial.print("DL Heartbeat Interval");
                      String cmd = "AT+SET_HEARTBEAT_INTERVAL=";
                      // uint16_t hbInt = (uint16_t)((numData[1] << 8) | numData[2]);
                      uint32_t hbInt = ((uint32_t)((numData[1] << 8) | numData[2]))*1000;
                      cmd += (String)hbInt; 
                      Serial.println(cmd);
                      handleCLICommand(cmd); 
                    }
                    break; 
                    
                    case  DL_DC_STATES_ID: {
                      Serial.println("DL DC States ");
                      String cmd = "AT+SET_HOT=";
                      for(int i = 0; i < 6; i++ )
                      {
                        cmd += ((numData[1] >> i) & 0x01) ? '1' : '0';
                        if(i < 5) cmd += ',';
                      }
                      Serial.println(cmd);
                      handleCLICommand(cmd); 
                    }
                    break; 
                    
                    case  DL_HOT_TIMEOUT_ID: {
                      Serial.println("DL Hot Timeout");
                      String cmd = "AT+SET_HOT_TIMEOUT=";
                      // uint16_t hotTimeout = (uint16_t)((numData[1] << 8) | numData[2]);
                      uint32_t hotTimeout = ((uint32_t)((numData[1] << 8) | numData[2]))*1000;
                      
                      cmd += (String)hotTimeout; 
                      Serial.println(cmd);
                      handleCLICommand(cmd); 
                    }
                    break; 
                    
                    case DL_PASSKEY_ID: {
                      Serial.println("DL Passkey");
                      String cmd = "AT+SET_PASSKEY="; 
                      for(int i = 0; i < (len/2)-1; i ++ )
                      {
                        Serial.print((char)numData[i+1]);
                        cmd += (char)numData[i+1];
                      }
                      Serial.println(" ");
                      Serial.println(cmd);// for debug only 
                      handleCLICommand(cmd); 
                    }
                    break; 
                    
                    
                    case DL_KEY_UPDATE_ENABLE_ID: 
                      Serial.print("Update credentials enable");
      
                    break; 

                    case DL_KEY_UPDATE_DISABLE_ID: 
                      Serial.print("Update credentials disable");

                    break; 

                    case DL_SAVE_SETTINGS_ID: 
                      Serial.print("Saving Settings");
          
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
            vTaskDelay(10 / portTICK_PERIOD_MS);
          }
          #ifdef ECHO_RX_REPLY
          Serial.println(reply); // Echo the received reply
          #endif
        }
      }
       
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); // Shorter delay for faster response
  }
 
}

void loraTxTask(void * parameters)
{ 
  /* KEYS Payload Test Code */
  TickType_t lastWakeTime = xTaskGetTickCount();
  while(1)
  {

    if(getLoraJoinStatus() && !rejoin) // checking connection
    {
      if ( isCheckingLink) {
          linkCheckDelayCounter++;
          if (linkCheckDelayCounter >= 10) { // ~10 seconds
              checkLinkc(); 
              sentCheckLinkReq++;
              linkCheckDelayCounter = 0;
          }
          if (sentCheckLinkReq >= NUM_OF_LINK_CHECK) {
              if (successfulLinkCheck == 0) {
                  Serial.println("[LoRa] Device is disconnected, need to rejoin");
                  setLoraJoinStatus(false);
                  rejoin = true; 
              } else {
                  Serial.println("[LoRa] Connection is Good");
              }
              isCheckingLink = false;
              successfulLinkCheck = 0;
              sentCheckLinkReq = 0;
              nextCheckLinkCounter = 0;
          }
      } else {
          nextCheckLinkCounter++;
          if(nextCheckLinkCounter >= NEXT_CHECK_LINK_AFTER) {
              isCheckingLink = true; 
          }
      }
    }else if(!getLoraJoinStatus() && rejoin)// rejoining
    {
      bool isJoined = false;
      Serial.println("[LoRa] Rejoining");
      while(!isJoined)
      {
        int i = 0; 
        unsigned long startTime; 
        startTime = millis();

        lorawanSerial.print("AT+JOIN=1\r");
        vTaskDelay(pdMS_TO_TICKS(10));
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
            vTaskDelay(pdMS_TO_TICKS(10)); 
        }
    
        vTaskDelay(pdMS_TO_TICKS(5000));
      }
       setLoraJoinStatus(true);
        rejoin = false;
    }
    vTaskDelayUntil(&lastWakeTime, 1000 / portTICK_PERIOD_MS);
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

  vTaskDelay(pdMS_TO_TICKS(3000));
  // empty rx buffer.
  // unsigned long startTime = millis();
  // String reply;
  // while ((millis() - startTime) < 1000) { // timeout 
  //   reply = lorawanSerial.readString();
  // }

  /* Check connection with at SLAVE */
  lorawanSerial.print("AT\r\n");
  vTaskDelay(pdMS_TO_TICKS(100));
  err = checkResponse(buffer, INVALID_CONNECTION); 
  if(err) return err;  
  Serial.print("Connection is valid \n\r"); 

  memset(buffer, 0, sizeof(buffer)); 

  /* Set APPEUI */
  Serial.printf("AT+APPEUI=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
  appEUI[0], appEUI[1], appEUI[2], appEUI[3], appEUI[4], appEUI[5], appEUI[6], appEUI[7] );
  
  sprintf(buffer,"AT+APPEUI=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
  appEUI[0], appEUI[1], appEUI[2], appEUI[3], appEUI[4], appEUI[5], appEUI[6], appEUI[7] );
  
  lorawanSerial.print(buffer);
  
  vTaskDelay(pdMS_TO_TICKS(100));
  memset(buffer, 0, sizeof(buffer)); 
  err = checkResponse(buffer, FAILED_WRITE_APPEUI); 
  if(err) return err;  
  Serial.print("APPEUI is set\n\r"); 

  /* Set devEUI */
  sprintf(buffer,"AT+DEUI=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
  devEUI[0], devEUI[1], devEUI[2], devEUI[3], devEUI[4], devEUI[5], devEUI[6], devEUI[7] );


  Serial.printf("AT+DEUI=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
  devEUI[0], devEUI[1], devEUI[2], devEUI[3], devEUI[4], devEUI[5], devEUI[6], devEUI[7] );


  lorawanSerial.print(buffer);
  vTaskDelay(pdMS_TO_TICKS(100));
  memset(buffer, 0, sizeof(buffer)); 
  err = checkResponse(buffer, FAILED_WRITE_DEUI); 
  if(err) return err;  
  Serial.print("DEUI is set\n\r"); 

  /* Set appKEY */
  Serial.printf("AT+APPKEY=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
          appKEY[0], appKEY[1], appKEY[2], appKEY[3], appKEY[4], appKEY[5], appKEY[6], appKEY[7],
          appKEY[8], appKEY[9], appKEY[10], appKEY[11], appKEY[12], appKEY[13], appKEY[14], appKEY[15]);

  sprintf(buffer,"AT+APPKEY=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
          appKEY[0], appKEY[1], appKEY[2], appKEY[3], appKEY[4], appKEY[5], appKEY[6], appKEY[7],
          appKEY[8], appKEY[9], appKEY[10], appKEY[11], appKEY[12], appKEY[13], appKEY[14], appKEY[15]);
  lorawanSerial.print(buffer);
  vTaskDelay(pdMS_TO_TICKS(100));
  memset(buffer, 0, sizeof(buffer)); 
  err = checkResponse(buffer, FAILED_WRITE_APPKEY); 
  if(err) return err;  
  Serial.print("APPKEY is set\n\r"); 

  /* Set nwkKEY */
  sprintf(buffer,"AT+NWKKEY=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
          appKEY[0], appKEY[1], appKEY[2], appKEY[3], appKEY[4], appKEY[5], appKEY[6], appKEY[7],
          appKEY[8], appKEY[9], appKEY[10], appKEY[11], appKEY[12], appKEY[13], appKEY[14], appKEY[15]);
  lorawanSerial.print(buffer);
  vTaskDelay(pdMS_TO_TICKS(100));
  memset(buffer, 0, sizeof(buffer)); 
  err = checkResponse(buffer, FAILED_WRITE_NWKKEY); 
  if(err) return err;  
  Serial.print("NWKKEY is set\n\r"); 

    /* Set appsKEY */
  sprintf(buffer,"AT+APPSKEY=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
          appKEY[0], appKEY[1], appKEY[2], appKEY[3], appKEY[4], appKEY[5], appKEY[6], appKEY[7],
          appKEY[8], appKEY[9], appKEY[10], appKEY[11], appKEY[12], appKEY[13], appKEY[14], appKEY[15]);
  lorawanSerial.print(buffer);
  vTaskDelay(pdMS_TO_TICKS(100));
  memset(buffer, 0, sizeof(buffer)); 
  err = checkResponse(buffer, FAILED_WRITE_APPSKEY); 
  if(err) return err;  
  Serial.print("APPSKEY is set\n\r"); 

  /* Set nwksKEY */
  sprintf(buffer,"AT+NWKSKEY=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n", 
          appKEY[0], appKEY[1], appKEY[2], appKEY[3], appKEY[4], appKEY[5], appKEY[6], appKEY[7],
          appKEY[8], appKEY[9], appKEY[10], appKEY[11], appKEY[12], appKEY[13], appKEY[14], appKEY[15]);
  lorawanSerial.print(buffer);
  vTaskDelay(pdMS_TO_TICKS(100));
  memset(buffer, 0, sizeof(buffer)); 
  err = checkResponse(buffer, FAILED_WRITE_NWKSKEY); 
  if(err) return err;  
  Serial.print("NWKSKEY is set\n\r"); 

    /* Set devADDR */
  sprintf(buffer,"AT+DADDR=%02X:%02X:%02X:%02X\r\n", 
          devADDR[0], devADDR[1], devADDR[2], devADDR[3]);
  lorawanSerial.print(buffer);
  vTaskDelay(pdMS_TO_TICKS(100));
  memset(buffer, 0, sizeof(buffer)); 
  err = checkResponse(buffer, FAILED_WRITE_DADDR); 
  if(err) return err;  
  Serial.print("Dev ADDR is set\n\r"); 

  /* Set devADDR */

  lorawanSerial.print("AT+CS\r");
  vTaskDelay(pdMS_TO_TICKS(100));
  err = checkResponse(buffer, FAILED_SAVE_SETTINGS); 
  if(err) return err;  
  Serial.print("Configuration is saved successfully \n\r"); 

  vTaskDelay(pdMS_TO_TICKS(100));
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
    vTaskDelay(pdMS_TO_TICKS(10));
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
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
 
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
  Serial.println("\n\rJOINED SUCCESSFULLY \n\r");
  return 0; 
  
}

int switchClass(void)
{
  vTaskDelay(pdMS_TO_TICKS(5000));
  char buffer[MAX_BUFFER_SIZE];
  int  err;  
  /* Switch Class */
  lorawanSerial.print("AT+CLASS=C\r");
  vTaskDelay(pdMS_TO_TICKS(10));
  err = checkResponse(buffer, FAILED_SWITCH_CLASS); 
  if(err) return err;  
  Serial.print("Switched to class C \n\r"); 


  /* Initial Transmission */
  lorawanSerial.print("AT+SEND=10:0:1234\r\n");
  vTaskDelay(pdMS_TO_TICKS(10));
  err = checkResponse(buffer, FAILED_TX_UNCONFIRMED); 
  if(err) return err;  
  Serial.print("Initial transmission done \n\r"); 

  vTaskDelay(pdMS_TO_TICKS(5000));
  return 0;
}


int checkLinkc(void)
{
  vTaskDelay(pdMS_TO_TICKS(10));
   lorawanSerial.print("AT+SEND=10:1:A7\r\n");
  vTaskDelay(pdMS_TO_TICKS(10));
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


/* Register callback  */
void registerOkCallback(void (*callback)(void)) {
    okCallback = callback;
}

void registerSendConfirmedCallback(void (*callback)(void)) {
    sendConfCallback = callback;
}

