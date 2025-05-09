#include "esp32-hal-gpio.h"
// SensorMonitorTask.cpp
#include "SensorMonitorTask.h"
#include "LoRaSenderTask.h"
#include <Arduino.h>
#include "Adafruit_SHT4x.h"
#include "PinConfig.h"
#include "SystemConfig.h"

//Instance creation
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

static TimerHandle_t hotAlarmTimer = NULL;

//Function Prototypes
uint16_t readLTC4015(uint8_t reg);

#define NUM_DCI 6

DryContactInput dryContacts[NUM_DCI] = {
  { DCI_1, false, false, true },  // trigger on HIGH
  { DCI_2, false, true, false },  // trigger on LOW
  { DCI_3, false, false, true },
  { DCI_4, false, false, true },
  { DCI_5, false, false, true },
  { DCI_6, false, false, true }
};

SensorReadings currentSensorReadings;

void enqueueTheftAlarm(TheftAlarmType type) {
  TheftAlarmPayload alarm;

  switch (type) {
    case HATCH_OPEN:
      strncpy(alarm.alarmType, "Hatch Open", sizeof(alarm.alarmType) - 1);
      break;
    case KEY_SILENT_ALARM:
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
    currentSensorReadings.dciStates[i] = dryContacts[i].state;
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
          bool prevState = dryContacts[i].state;
          dryContacts[i].state = currentState;

          // Update shared readings first before any snapshot
          currentSensorReadings.dciStates[i] = currentState;

          bool isRisingEdge = (prevState == LOW && currentState == HIGH);
          bool isFallingEdge = (prevState == HIGH && currentState == LOW);

          bool hotTriggered = false;

          if (dryContacts[i].isHot) {
            if ((dryContacts[i].triggerOnHigh && isRisingEdge) ||
                (!dryContacts[i].triggerOnHigh && isFallingEdge)) {
              if (!xTimerIsTimerActive(hotAlarmTimer)) {
                xTimerStart(hotAlarmTimer, 0);
                Serial.printf("[HOT] Timer started by DCI_%d\n", i + 1);
              }
              Serial.printf("[HOT] Channel DCI_%d Triggered\n", i + 1);
              hotTriggered = true;
            }
          }

          // Log DCI edge regardless of HOT trigger
          Serial.printf("[Event] DCI_%d %s (Trigger on %s)\n",
                        i + 1,
                        currentState == HIGH ? "HIGH" : "LOW",
                        dryContacts[i].triggerOnHigh ? "High" : "Low");

          // Enqueue to Events Queue for all edges
          EventsPayload evt;
          SensorReadings snapshot = getSensorReadings();  // now includes updated DCI states
          memcpy(evt.dciStates, snapshot.dciStates, sizeof(evt.dciStates));
          evt.temperature = snapshot.temperature;
          evt.humidity = snapshot.humidity;

          if (xQueueSend(eventsQueue, &evt, 0) == pdPASS) {
            Serial.println("[Event] Enqueued due to DCI edge");
          } else {
            Serial.println("[Event] Queue Full - event not sent");
          }
        }
      }
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
    Serial.println(sht4.readSerial(), HEX);

    // Set SHT Precision
    sht4.setPrecision(SHT4X_MED_PRECISION);
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

void powerMonitorTask(void* pvParameters) {
  while (1) {
    PowerPayload payload;

    payload.vbat = readLTC4015(REG_VBAT) * 192.264 / 1000000.0 * 4.0;
    payload.vin  = readLTC4015(REG_VIN) * 1.648 / 1000.0;
    payload.vsys = readLTC4015(REG_VSYS) * 1.648 / 1000.0;
    payload.ibat = readLTC4015(REG_IBAT) * 1.46487 / 1000000.0;
    payload.iin  = readLTC4015(REG_IIN) * 1.46487 / 1000000.0;
    payload.DCO2State = digitalRead(DCO_2);

    Serial.printf("[LTC4015] VBAT: %.2fV | VIN: %.2fV | VSYS: %.2fV | IBAT: %.3fA | IIN: %.3fA | DCO2: %d \n",
                  payload.vbat, payload.vin, payload.vsys, payload.ibat, payload.iin, payload.DCO2State);

    if (xQueueSend(powerPayloadQueue, &payload, 0) == pdPASS) {
      Serial.println("[Power] Enqueued power payload");
    } else {
      Serial.println("[Power] Queue full, skipping enqueue");
    }

    vTaskDelay(pdMS_TO_TICKS(10000));  // every 10 seconds
  }
}

uint16_t readLTC4015(uint8_t reg) {
  Wire.beginTransmission(LTC4015_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);  // repeated start
  Wire.requestFrom(LTC4015_ADDR, 2);
  if (Wire.available() == 2) {
    uint16_t lsb = Wire.read();
    uint16_t msb = Wire.read();
    return (msb << 8) | lsb;
  }
  return 0xFFFF;
}


void createSensorTasks() {

  hotAlarmTimer = xTimerCreate(
    "HotAlarmTimer",
    pdMS_TO_TICKS(hotAlarmDurationMs),
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
    3,
    NULL,
    0);

  xTaskCreatePinnedToCore(
    monitorSHTSensorTask,
    "SHTSensorMonitor",
    4096,
    NULL,
    2,
    NULL,
    0);

     xTaskCreatePinnedToCore(
    powerMonitorTask,
    "PowerMonitor",
    4096,
    NULL,
    1,
    NULL,
    0
  );

  enqueueHeartbeatEvery(heartbeatInterval);
}


void stopHotAlarmTimer() {
  if (xTimerIsTimerActive(hotAlarmTimer)) {
    xTimerStop(hotAlarmTimer, 0);
    Serial.println("[HOT] Timer stopped globally.");
  }
  digitalWrite(DCO_1, RELAY_OFF);
}
