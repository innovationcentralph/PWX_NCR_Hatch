// LoRaSenderTask.h
#pragma once

#include <Arduino.h>

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
};

// Payload structure for Heartbeat
struct HeartbeatPayload {
  bool dciStates[MAX_DCI];
  float temperature;
  float humidity;
};

extern QueueHandle_t theftAlarmQueue;
extern QueueHandle_t eventsQueue;
extern QueueHandle_t heartbeatQueue;

void createLoRaQueues();
void createLoRaSenderTask();

void sendTheftAlarmPayload(const TheftAlarmPayload& payload);
void sendEventsPayload(const EventsPayload& payload);
void sendHeartbeatPayload(const HeartbeatPayload& payload);