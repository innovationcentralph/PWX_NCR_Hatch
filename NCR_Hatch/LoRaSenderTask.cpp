// LoRaSenderTask.cpp
#include "LoRaSenderTask.h"
#include "SensorMonitorTask.h"
#include "SystemConfig.h"
#include "lorawan_handler.h"
#include "lorawan.h"
#include <Arduino.h>

extern bool isLoRaReady;

QueueHandle_t theftAlarmQueue;
QueueHandle_t eventsQueue;
QueueHandle_t heartbeatQueue;
QueueHandle_t passKeyQueue;
QueueHandle_t powerPayloadQueue;

HeartbeatPayload _heartbeatPayload;
EventsPayload _eventsPayload;
TheftAlarmPayload _alarmPayload;
PassKeyPayload _passkeyPayload;
PowerPayload _powerPayload;

void createLoRaQueues() {
  theftAlarmQueue = xQueueCreate(5, sizeof(TheftAlarmPayload));
  eventsQueue = xQueueCreate(5, sizeof(EventsPayload));
  heartbeatQueue = xQueueCreate(5, sizeof(HeartbeatPayload));
  passKeyQueue = xQueueCreate(5, sizeof(PassKeyPayload));
  powerPayloadQueue = xQueueCreate(1, sizeof(PowerPayload));
}

void loraSenderTask(void* pvParameters) {
  const TickType_t interval = pdMS_TO_TICKS(3000);
  TickType_t lastWakeTime = xTaskGetTickCount();

  while (1) {
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(loraSendInterval));

    if(getLoraJoinStatus() && !checkIfLoRaTxBusy()){
      // Priority 1: Theft Alarm
      if (xQueueReceive(theftAlarmQueue, &_alarmPayload, 0) == pdPASS) {
        sendTheftAlarmPayload(_alarmPayload);

        // Print theft alarm data
        Serial.printf("[LoRa] Theft Alarm Triggered: %s\n", _alarmPayload.alarmType);
        for (int i = 0; i < MAX_DCI; i++) {
          Serial.printf("  DCI_%d: %s\n", i + 1, _alarmPayload.dciStates[i] ? "HIGH" : "LOW");
        }


        continue;
      }

      // Priority 2: Passkey
      if (xQueueReceive(passKeyQueue, &_passkeyPayload, 0) == pdPASS) {
        sendPasskeyPayload(_passkeyPayload);

        // Print PassKey data
        Serial.printf("[LoRa] Pass Key Payload: \r\n");
        Serial.printf("[LoRa] Status: %s\r\n", _passkeyPayload.passkeyStat == CORRECT_PASSKEY ? "CORRECT" : "INCORRECT");
        Serial.printf("[LoRa] Type: %s\r\n", _passkeyPayload.passkeyType == SILENT ? "SILENT" : "NOT SILENT");
        Serial.printf("[LoRa] Length: %d\r\n", _passkeyPayload.len);

        Serial.print("[LoRa] PassKey: ");
        for (uint8_t x = 0; x < _passkeyPayload.len; x++) {
          Serial.print(_passkeyPayload.passk[x]);
        }
        Serial.println();

        continue;
      }

      // Priority 3: Events

      if (xQueueReceive(eventsQueue, &_eventsPayload, 0) == pdPASS) {
        sendEventsPayload(_eventsPayload);

        // Print events data
        Serial.println("[LoRa] Events Payload Data:");
        for (int i = 0; i < MAX_DCI; i++) {
          Serial.printf("  DCI_%d: %s\n", i + 1, _eventsPayload.dciStates[i] ? "HIGH" : "LOW");
        }
        Serial.printf("  Temperature: %.2f °C\n", _eventsPayload.temperature);
        Serial.printf("     Humidity: %.2f RH\n", _eventsPayload.humidity);

        continue;
      }

      // Priority 4: Heartbeat

      if (xQueueReceive(heartbeatQueue, &_heartbeatPayload, 0) == pdPASS) {
        sendHeartbeatPayload(_heartbeatPayload);

        // Print heartbeat data
        Serial.println("[LoRa] Heartbeat Payload Data:");
        for (int i = 0; i < MAX_DCI; i++) {
          Serial.printf("  DCI_%d: %s\n", i + 1, _heartbeatPayload.dciStates[i] ? "HIGH" : "LOW");
        }
        Serial.printf("  Temperature: %.2f °C\n", _heartbeatPayload.temperature);
        Serial.printf("     Humidity: %.2f °C\n", _heartbeatPayload.humidity);

        continue;
      }

      // Priority 5: Diagnostics

      if (xQueueReceive(powerPayloadQueue, &_powerPayload, 0) == pdPASS) {
        sendPowerPayload(_powerPayload); 



        Serial.println("[LoRa] Power Payload Data:");
        Serial.printf("VBAT=%.2f,VIN=%.2f,VSYS=%.2f,IBAT=%.2f,IIN=%.2f \r\n",
                 _powerPayload.vbat,
                 _powerPayload.vin,
                 _powerPayload.vsys,
                 _powerPayload.ibat,
                 _powerPayload.iin);

        continue;
      }

      // If all queues are empty
      Serial.println("[LoRa] No data to send. All queues are empty.");

    }else{
      Serial.println("Lora Handler Busy at the moment");
    }

    
  }
}

void sendTheftAlarmPayload(const TheftAlarmPayload& payload) {
  alarmPayload_s* alarmPayloadPtr;
  alarmPayloadPtr = getAlarmPayloadInstance();

  alarmPayloadPtr->dryContactStat.dc1 = _alarmPayload.dciStates[0];
  alarmPayloadPtr->dryContactStat.dc2 = _alarmPayload.dciStates[1];
  alarmPayloadPtr->dryContactStat.dc3 = _alarmPayload.dciStates[2];
  alarmPayloadPtr->dryContactStat.dc4 = _alarmPayload.dciStates[3];
  alarmPayloadPtr->dryContactStat.dc5 = _alarmPayload.dciStates[4];
  alarmPayloadPtr->dryContactStat.dc6 = _alarmPayload.dciStates[5];

  if (strcmp(_alarmPayload.alarmType, "Hatch Open") == 0) {
    if (isLoRaReady) {
      processUplink(ALARM_THEFT, CONFIRMED_UPLINK);
    }
  } else if (strcmp(_alarmPayload.alarmType, "Silent Alarm Triggered") == 0) {
    if (isLoRaReady) {
      processUplink(SILENT_ALARM, CONFIRMED_UPLINK);
    }
  }

  Serial.println("[LoRa] Sent Theft Alarm Payload");
}

void sendEventsPayload(const EventsPayload& payload) {

  eventsPayload_s* _eventPayloadPtr;
  _eventPayloadPtr = getEventsPayloadInstance();
  _eventPayloadPtr->temperature.all = (uint16_t)(_eventsPayload.temperature * 10);
  _eventPayloadPtr->humidity.all = (uint16_t)(_eventsPayload.humidity * 10);
  _eventPayloadPtr->dryContactStat.dc1 = _eventsPayload.dciStates[0];
  _eventPayloadPtr->dryContactStat.dc2 = _eventsPayload.dciStates[1];
  _eventPayloadPtr->dryContactStat.dc3 = _eventsPayload.dciStates[2];
  _eventPayloadPtr->dryContactStat.dc4 = _eventsPayload.dciStates[3];
  _eventPayloadPtr->dryContactStat.dc5 = _eventsPayload.dciStates[4];
  _eventPayloadPtr->dryContactStat.dc6 = _eventsPayload.dciStates[5];
  if (isLoRaReady) {
    processUplink(EVENTS, CONFIRMED_UPLINK);
    Serial.println("[LoRa] Sent Events Payload");
  }
}

void sendHeartbeatPayload(const HeartbeatPayload& payload) {

  hearbeatPayload_s* _hbPayloadPtr;
  _hbPayloadPtr = heartbeatPayloadInstance();
  _hbPayloadPtr->temperature.all = (uint16_t)(_heartbeatPayload.temperature * 10);
  _hbPayloadPtr->humidity.all = (uint16_t)(_heartbeatPayload.humidity * 10);
  _hbPayloadPtr->siren = 0;  // placeholder
  _hbPayloadPtr->dryContactStat.dc1 = _heartbeatPayload.dciStates[0];
  _hbPayloadPtr->dryContactStat.dc2 = _heartbeatPayload.dciStates[1];
  _hbPayloadPtr->dryContactStat.dc3 = _heartbeatPayload.dciStates[2];
  _hbPayloadPtr->dryContactStat.dc4 = _heartbeatPayload.dciStates[3];
  _hbPayloadPtr->dryContactStat.dc5 = _heartbeatPayload.dciStates[4];
  _hbPayloadPtr->dryContactStat.dc6 = _heartbeatPayload.dciStates[5];

  if (isLoRaReady) {
    processUplink(HEARTBEAT, UNCONFIRMED_UPLINK);
    Serial.println("[LoRa] Sent Heartbeat Payload");
  }
}

void sendPasskeyPayload(const PassKeyPayload& payload) {

  keysPayload_s* _keysPayloadPtr;
  _keysPayloadPtr = keysPayloadInstance();

  _keysPayloadPtr->passkeyStat = _passkeyPayload.passkeyStat;
  _keysPayloadPtr->passkeyType = _passkeyPayload.passkeyType;
  _keysPayloadPtr->len = _passkeyPayload.len;

  memcpy(_keysPayloadPtr->passk, payload.passk, payload.len);

  if (isLoRaReady) {
    processUplink(KEYS, UNCONFIRMED_UPLINK);
    Serial.println("[LoRa] Sent PassKey Payload");
  }
}

void sendPowerPayload(const PowerPayload& payload) {

  diagnosticPayload_s* _powerPayloadPtr;
  _powerPayloadPtr = getDiagnosticPayloadInstance();
  _powerPayloadPtr->dcVolt.all = _powerPayload.vin * 10;
  _powerPayloadPtr->dcCurr.all = _powerPayload.iin * 10;
  _powerPayloadPtr->battVolt.all = _powerPayload.vbat * 10;
  _powerPayloadPtr->battCurr.all = _powerPayload.vin * 10;
  _powerPayloadPtr->dcoState = _powerPayload.DCO2State;


  if (isLoRaReady) {
    processUplink(DIAGNOSTIC, UNCONFIRMED_UPLINK);
    Serial.println("[LoRa] Sent Power Payload");
  }
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
