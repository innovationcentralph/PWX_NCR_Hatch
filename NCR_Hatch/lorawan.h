#ifndef LORAWAN_H
#define LORAWAN_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h> 
#include <string.h>
#include <stdlib.h>
#define MAX_LORA_BUFFER_SIZE 50

typedef enum{
	UNCONFIRMED_UPLINK= 0, 
	CONFIRMED_UPLINK // (need to return ack)
}UPLINK_TYPE; 

typedef enum  
{ 
	ALARM_THEFT= 0xA1,// (Confirmed) 
	SILENT_ALARM, //(Confirmed uplink) 
	HEARTBEAT,// (Un confirmed uplink every 15mins)
	DIAGNOSTIC,
	EVENTS, 

} PAYLOAD_TYPE;

typedef enum 
{
    NO_EVENT=0x00,  
    DC_CHANGE_OF_STATE,
    TEMP_THRESHOLD, 
    HUM_THRESHOLD,
    
}EVENT_TYPE; 

typedef union 
{
    uint8_t all;
    struct{
        bool dc1:1; 
        bool dc2:1;
        bool dc3:1; 
        bool dc4:1; 
        bool dc5:1;
        bool dc6:1; 
        bool reserved1:1; 
        bool reserved2:1; 
    };
}dryContact_u; 

typedef union
{
    int16_t all;
    struct
    {
        int8_t upper; 
        int8_t lower; 
    };
}int16; 

/* Payload Structure */
typedef struct
{
    dryContact_u dryContactStat; 
}alarmPayload_s; 
typedef struct
{
    EVENT_TYPE eventOccured;
    int16 temperature; 
    int16 humidity; 
    dryContact_u dryContactStat; 
}eventsPayload_s; 

typedef struct
{
    int16 dcVolt; 
    int16 dcCurr; 
    int16 battVolt; 
    int16 battCurr;
}diagnosticPayload_s; 

typedef struct
{
    int16 temperature; 
    int16 humidity; 
    uint8_t siren;
    dryContact_u dryContactStat; 
}hearbeatPayload_s; 



static alarmPayload_s alarmPayload; 
static eventsPayload_s eventsPayload; 
static diagnosticPayload_s diagnosticPayload; 
static hearbeatPayload_s heartbeatPayload; 



/*To access structure outside the code  */
alarmPayload_s * getAlarmPayloadInstance();
eventsPayload_s * getEventsPayloadInstance(); 
diagnosticPayload_s * getDiagnosticPayloadInstance();
hearbeatPayload_s * heartbeatPayloadInstance();

typedef void (*serial_tx_function_t)(const char*);

void lora_serial_init(serial_tx_function_t logger);

int processUplink(PAYLOAD_TYPE payloadType, UPLINK_TYPE uplinkType);

int loraTransmit(uint8_t * buffer, uint8_t len, int port, UPLINK_TYPE uplinkType);


#endif