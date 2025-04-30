// LoRaSenderTask.cpp
#include "LoRaSenderTask.h"
#include "SensorMonitorTask.h"
#include <Arduino.h>

QueueHandle_t theftAlarmQueue;
QueueHandle_t eventsQueue;
QueueHandle_t heartbeatQueue;

void createLoRaQueues() {
  theftAlarmQueue = xQueueCreate(5, sizeof(TheftAlarmPayload));
  eventsQueue = xQueueCreate(5, sizeof(EventsPayload));
  heartbeatQueue = xQueueCreate(5, sizeof(HeartbeatPayload));
}

void loraSenderTask(void* pvParameters) {
  const TickType_t interval = pdMS_TO_TICKS(3000);  
  TickType_t lastWakeTime = xTaskGetTickCount();

  while (1) {
    vTaskDelayUntil(&lastWakeTime, interval);

    // Priority 1: Theft Alarm
    TheftAlarmPayload alarmPayload;
    if (xQueueReceive(theftAlarmQueue, &alarmPayload, 0) == pdPASS) {
      sendTheftAlarmPayload(alarmPayload);

      // Print theft alarm data
      Serial.printf("[LoRa] Theft Alarm Triggered: %s\n", alarmPayload.alarmType);
      for (int i = 0; i < MAX_DCI; i++) {
        Serial.printf("  DCI_%d: %s\n", i + 1, alarmPayload.dciStates[i] ? "HIGH" : "LOW");
      }


      continue;
    }

    // Priority 2: Events
    EventsPayload eventsPayload;
    if (xQueueReceive(eventsQueue, &eventsPayload, 0) == pdPASS) {
      sendEventsPayload(eventsPayload);

      // Print events data
      Serial.println("[LoRa] Events Payload Data:");
      for (int i = 0; i < MAX_DCI; i++) {
        Serial.printf("  DCI_%d: %s\n", i + 1, eventsPayload.dciStates[i] ? "HIGH" : "LOW");
      }
      Serial.printf("  Temperature: %.2f °C\n", eventsPayload.temperature);

      continue;
    }

    // Priority 3: Heartbeat
    HeartbeatPayload heartbeatPayload;
    if (xQueueReceive(heartbeatQueue, &heartbeatPayload, 0) == pdPASS) {
      sendHeartbeatPayload(heartbeatPayload);

      // Print heartbeat data
      Serial.println("[LoRa] Heartbeat Payload Data:");
      for (int i = 0; i < MAX_DCI; i++) {
        Serial.printf("  DCI_%d: %s\n", i + 1, heartbeatPayload.dciStates[i] ? "HIGH" : "LOW");
      }
      Serial.printf("  Temperature: %.2f °C\n", heartbeatPayload.temperature);
      Serial.printf("     Humidity: %.2f °C\n", heartbeatPayload.humidity);

      continue;
    }

    // If all queues are empty
    //Serial.println("[LoRa] No data to send. All queues are empty.");
  }
}

void sendTheftAlarmPayload(const TheftAlarmPayload& payload) {
  Serial.println("[LoRa] Sent Theft Alarm Payload");
}

void sendEventsPayload(const EventsPayload& payload) {
  Serial.println("[LoRa] Sent Events Payload");
}

void sendHeartbeatPayload(const HeartbeatPayload& payload) {
  Serial.println("[LoRa] Sent Heartbeat Payload");
}

void createLoRaSenderTask() {
  createLoRaQueues();
  xTaskCreatePinnedToCore(
    loraSenderTask,
    "LoRaSender",
    4096,
    NULL,
    1,
    NULL,
    1);
}
