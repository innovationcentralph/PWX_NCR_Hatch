// LoRaSenderTask.h
#pragma once

#include <Arduino.h>
#include "lorawan.h"

#define MAX_DCI 6

// Payload structure for Theft Alarm
struct TheftAlarmPayload {
  char alarmType[32];       // e.g., "Open Hot DC", "Silent Alarm Triggered"
  bool dciStates[MAX_DCI];  // DCI states snapshot
};

// Payload structure for Events
struct EventsPayload {
  bool dciStates[MAX_DCI];
  float temperature;
  float humidity;
};

// Payload structure for Heartbeat
struct HeartbeatPayload {
  bool dciStates[MAX_DCI];
  float temperature;
  float humidity;
};

// Payload structure for Passkey Entry
typedef struct PassKeyPayload
{
    PASSKEY_STAT passkeyStat; 
    PASSKEY_TYPE passkeyType; 
    uint8_t len;
    char passk[8];
};

struct PowerPayload {
  float vbat;    // Battery voltage
  float vin;     // Input voltage
  float vsys;    // System voltage
  float ibat;    // Battery current
  float iin;     // Input current
  bool DCO2State;
};

extern QueueHandle_t theftAlarmQueue;
extern QueueHandle_t eventsQueue;
extern QueueHandle_t heartbeatQueue;
extern QueueHandle_t passKeyQueue;
extern QueueHandle_t powerPayloadQueue;

void createLoRaQueues();
void createLoRaSenderTask();

void sendTheftAlarmPayload(const TheftAlarmPayload& payload);
void sendEventsPayload(const EventsPayload& payload);
void sendHeartbeatPayload(const HeartbeatPayload& payload);
void sendPasskeyPayload(const PassKeyPayload& payload);
void sendPowerPayload(const PowerPayload& payload);