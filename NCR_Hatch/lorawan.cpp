// Online C compiler to run C program online

#include "lorawan.h"

static serial_tx_function_t serial_tx= 0;


/*To access structure outside the code  */
alarmPayload_s * getAlarmPayloadInstance()
{
    return &alarmPayload; 

}

eventsPayload_s * getEventsPayloadInstance()
{
    return &eventsPayload; 
}

diagnosticPayload_s * getDiagnosticPayloadInstance()
{
    return &diagnosticPayload; 
}

/*To access structure outside the code  */
hearbeatPayload_s * heartbeatPayloadInstance()
{
    return &heartbeatPayload; 
}
keysPayload_s * keysPayloadInstance()
{
    return &keysPayload;
}


void lora_serial_init(serial_tx_function_t logger) {
    serial_tx = logger;
}


int processUplink(PAYLOAD_TYPE payloadType, UPLINK_TYPE uplinkType)
{ 
    uint8_t txBuffer[MAX_LORA_BUFFER_SIZE];
    int len = 0, err; 
    switch(payloadType)
    {
         case ALARM_THEFT:
            // printf("Alarm\n\r"); 
            txBuffer[len++]=(uint8_t)ALARM_THEFT;
            txBuffer[len++]=(uint8_t)alarmPayload.dryContactStat.all;
            break;
        case SILENT_ALARM:
            // printf("Silent Alarm\n\r"); 
            txBuffer[len++]=(uint8_t)SILENT_ALARM; 
            txBuffer[len++]=(uint8_t)alarmPayload.dryContactStat.all;
            break;
        case HEARTBEAT:
            // printf("Hearbeat\n\r"); 
            txBuffer[len++]=(uint8_t)HEARTBEAT;
            txBuffer[len++]=heartbeatPayload.temperature.upper; 
            txBuffer[len++]=heartbeatPayload.temperature.lower; 
            txBuffer[len++]=heartbeatPayload.humidity.upper; 
            txBuffer[len++]=heartbeatPayload.humidity.lower; 
            txBuffer[len++]=heartbeatPayload.siren; 
            txBuffer[len++]=heartbeatPayload.dryContactStat.all; 

            break;
        case DIAGNOSTIC:
            // printf("Diagnostic\n\r"); 
            txBuffer[len++]=(uint8_t)DIAGNOSTIC;
            txBuffer[len++]=diagnosticPayload.dcVolt.upper; 
            txBuffer[len++]=diagnosticPayload.dcVolt.lower; 
            txBuffer[len++]=diagnosticPayload.dcCurr.upper; 
            txBuffer[len++]=diagnosticPayload.dcCurr.lower; 
            txBuffer[len++]=diagnosticPayload.battVolt.upper; 
            txBuffer[len++]=diagnosticPayload.battVolt.lower; 
            txBuffer[len++]=diagnosticPayload.battCurr.upper; 
            txBuffer[len++]=diagnosticPayload.battCurr.lower; 
            break;
        case EVENTS:
            // printf("Events\n\r"); 
            txBuffer[len++]= (uint8_t)EVENTS; 
            txBuffer[len++]= eventsPayload.eventOccured; 
            txBuffer[len++]= eventsPayload.temperature.upper; 
            txBuffer[len++]= eventsPayload.temperature.lower; 
            txBuffer[len++]= eventsPayload.humidity.upper; 
            txBuffer[len++]= eventsPayload.humidity.lower; 
            txBuffer[len++]= (uint8_t)eventsPayload.dryContactStat.all; 
            break;
        case KEYS:
            // printf("Events\n\r"); 
            txBuffer[len++]= (uint8_t)KEYS; 
            txBuffer[len++]= keysPayload.passkeyStat; 
            txBuffer[len++]= keysPayload.passk[0];
            txBuffer[len++]= keysPayload.passk[1];
            txBuffer[len++]= keysPayload.passk[2];
            txBuffer[len++]= keysPayload.passk[3];
            if(keysPayload.len > 4) txBuffer[len++]= keysPayload.passk[4];
            if(keysPayload.len > 5) txBuffer[len++]= keysPayload.passk[5];
            if(keysPayload.len > 6) txBuffer[len++]= keysPayload.passk[6];
            if(keysPayload.len > 7) txBuffer[len++]= keysPayload.passk[7];

            break;
        default:
        break; 

    }
    err  = loraTransmit(txBuffer, len, 2, uplinkType);

    return 0; 
} 




int loraTransmit(uint8_t * tx_buffer, uint8_t len, int port,UPLINK_TYPE uplinkType)
{
//   char *loraBuffer = (char *)malloc(MAX_BUFFER_SIZE);
//   char *hexString = (char *)malloc(2 * len + 1);
  char hexString[150];  
  char loraBuffer[300];  

  memset(hexString, 0, sizeof(hexString));
  memset(loraBuffer, 0, sizeof(loraBuffer));

  // Convert data to hex string
  for(int i = 0; i < len && i*2 < sizeof(hexString)-1; i++) {
      snprintf(hexString + (i*2), 3, "%02X", tx_buffer[i]);
  }

  snprintf(loraBuffer, MAX_LORA_BUFFER_SIZE, "AT+SEND=%d:%d:%s\r\n", port, uplinkType, hexString);
  
  serial_tx(loraBuffer); 
//   free(loraBuffer);
//   free(hexString);
  return 0; 
}