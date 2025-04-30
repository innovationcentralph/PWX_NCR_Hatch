// SensorMonitorTask.cpp
#include "SensorMonitorTask.h"
#include "LoRaSenderTask.h"
#include <Arduino.h>
#include "Adafruit_SHT4x.h"
#include "PinConfig.h"

//Instance creation
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

static TimerHandle_t hotAlarmTimer = NULL;

#define NUM_DCI 6

DryContactInput dryContacts[NUM_DCI] = {
  { DCI_1, false, false },
  { DCI_2, false, true },
  { DCI_3, false, false },
  { DCI_4, false, false },
  { DCI_5, false, false },
  { DCI_6, false, false }
};

SensorReadings currentSensorReadings;

void enqueueTheftAlarm(TheftAlarmType type) {
  TheftAlarmPayload alarm;

  switch (type) {
    case HATCH_OPEN:
      strncpy(alarm.alarmType, "Hatch Open", sizeof(alarm.alarmType) - 1);
      break;
    case SILENT_ALARM:
      strncpy(alarm.alarmType, "Silent Alarm Triggered", sizeof(alarm.alarmType) - 1);
      break;
    default:
      strncpy(alarm.alarmType, "Unknown", sizeof(alarm.alarmType) - 1);
      break;
  }

  alarm.alarmType[sizeof(alarm.alarmType) - 1] = '\0';

  SensorReadings snapshot = getSensorReadings();
  memcpy(alarm.dciStates, snapshot.dciStates, sizeof(alarm.dciStates));

  if (xQueueSend(theftAlarmQueue, &alarm, 0) == pdPASS) {
    Serial.printf("[ALARM] Enqueued: %s\n", alarm.alarmType);
  } else {
    Serial.println("[ALARM] Queue Full - alarm not sent");
  }
}

void monitorDryContactsTask(void *pvParameters) {
  const int debounceDelay = 50;  // ms
  bool lastReadState[NUM_DCI];
  unsigned long lastDebounceTime[NUM_DCI];

  // Init
  for (int i = 0; i < NUM_DCI; i++) {
    dryContacts[i].state = digitalRead(dryContacts[i].pin);
    lastReadState[i] = dryContacts[i].state;
    lastDebounceTime[i] = millis();
  }

  while (1) {
    for (int i = 0; i < NUM_DCI; i++) {
      bool currentState = digitalRead(dryContacts[i].pin);

      if (currentState != lastReadState[i]) {
        lastDebounceTime[i] = millis();
        lastReadState[i] = currentState;
      }

      if ((millis() - lastDebounceTime[i]) > debounceDelay) {
        if (currentState != dryContacts[i].state) {
          dryContacts[i].state = currentState;

          bool hotTriggered = false;

          // Print message
          if (dryContacts[i].isHot && currentState == HIGH) {
            if (!xTimerIsTimerActive(hotAlarmTimer)) {
              xTimerStart(hotAlarmTimer, 0);
              Serial.printf("[HOT] Timer started by DCI_%d\n", i + 1);
            }
            Serial.printf("[HOT] Channel DCI_%d Opened\n", i + 1);
            hotTriggered = true;
          } else {
            Serial.printf("DCI_%d changed to %s\n", i + 1, currentState == HIGH ? "HIGH" : "LOW");
          }

          // Enqueue to Events Queue only if not HOT
          if (!hotTriggered) {
            EventsPayload evt;
            SensorReadings snapshot = getSensorReadings();
            memcpy(evt.dciStates, snapshot.dciStates, sizeof(evt.dciStates));
            evt.temperature = snapshot.temperature;

            if (xQueueSend(eventsQueue, &evt, 0) == pdPASS) {
              Serial.println("[Event] Enqueued due to DCI change");
            } else {
              Serial.println("[Event] Queue Full - event not sent");
            }
          }
        }
      }

      // Update shared readings
      currentSensorReadings.dciStates[i] = dryContacts[i].state;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void monitorSHTSensorTask(void *pvParameters) {

  // Initialize SHT40
  Serial.println("Adafruit SHT4x test");
  if (!sht4.begin()) {
    Serial.println("Couldn't find SHT4x");
  } else {
    Serial.println("Found SHT4x sensor");
    Serial.print("Serial number 0x");
    Serial.println(sht4.readSerial(), HEX);  // To Ensure communication between MCU and SHT40

    // Set SHT Precision
    sht4.setPrecision(SHT4X_MED_PRECISION);  // The higher the precision, the longer it takes to read
    sht4.setHeater(SHT4X_NO_HEATER);
    switch (sht4.getPrecision()) {
      case SHT4X_HIGH_PRECISION:
        Serial.println("High precision");
        break;
      case SHT4X_MED_PRECISION:
        Serial.println("Med precision");
        break;
      case SHT4X_LOW_PRECISION:
        Serial.println("Low precision");
        break;
    }
  }


  while (1) {
    sensors_event_t humidity, temp;
    sht4.getEvent(&humidity, &temp);

    currentSensorReadings.temperature = temp.temperature;
    currentSensorReadings.humidity = humidity.relative_humidity;

    // Serial.printf("[TEMP/HUM] Temperature: %.2f °C | Humidity: %.2f %%RH\n",
    //               currentSensorReadings.temperature,
    //               currentSensorReadings.humidity);

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

SensorReadings getSensorReadings() {
  return currentSensorReadings;
}

void enqueueHeartbeatEvery(TickType_t intervalMs) {
  xTaskCreatePinnedToCore(
    [](void *param) {
      TickType_t last = xTaskGetTickCount();
      TickType_t interval = *(TickType_t *)param;
      while (1) {
        vTaskDelayUntil(&last, interval);

        HeartbeatPayload hb;
        SensorReadings snapshot = getSensorReadings();
        memcpy(hb.dciStates, snapshot.dciStates, sizeof(hb.dciStates));
        hb.temperature = snapshot.temperature;
        hb.humidity = snapshot.humidity;

        xQueueSend(heartbeatQueue, &hb, 0);
        Serial.println("[Heartbeat] Enqueued");
      }
    },
    "HeartbeatEnqueuer",
    2048,
    new TickType_t(pdMS_TO_TICKS(intervalMs)),
    1,
    NULL,
    1);
}

void createSensorTasks() {

  hotAlarmTimer = xTimerCreate(
    "HotAlarmTimer",
    pdMS_TO_TICKS(10000),  // 10 seconds
    pdFALSE,
    NULL,
    [](TimerHandle_t xTimer) {
      Serial.println("[HOT] Timer expired — ALARM would trigger here.");
      digitalWrite(DCO_1, RELAY_ON);
      enqueueTheftAlarm(HATCH_OPEN);
    });


  // Initialize DCI pins
  for (int i = 0; i < NUM_DCI; i++) {
    pinMode(dryContacts[i].pin, INPUT);
  }

  xTaskCreatePinnedToCore(
    monitorDryContactsTask,
    "DryContactMonitor",
    2048,
    NULL,
    2,
    NULL,
    0);

  xTaskCreatePinnedToCore(
    monitorSHTSensorTask,
    "SHTSensorMonitor",
    4096,
    NULL,
    1,
    NULL,
    0);

  enqueueHeartbeatEvery(15000);  // Send heartbeat every 15 seconds
}


void stopHotAlarmTimer() {
  if (xTimerIsTimerActive(hotAlarmTimer)) {
    xTimerStop(hotAlarmTimer, 0);
    Serial.println("[HOT] Timer stopped globally.");
  }
  digitalWrite(DCO_1, RELAY_OFF);
}
