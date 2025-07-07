#include "esp32-hal-gpio.h"
// SensorMonitorTask.cpp
#include "SensorMonitorTask.h"
#include "LoRaSenderTask.h"
#include <Arduino.h>
#include "Adafruit_SHT4x.h"
#include "PinConfig.h"
#include "SystemConfig.h"
#include <DFRobot_LIS2DW12.h>
#include "ADS1X15.h"

#define SMOKE_THRESHOLD 335//409


//Instance creation
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
DFRobot_LIS2DW12_I2C acce(&Wire, 0x18);
ADS1015 ADS(0x48);


static TimerHandle_t hotAlarmTimer = NULL;
static bool hotCooldownActive = false;
static TimerHandle_t hotCooldownTimer = NULL;

CompressedEventsPayload _compressedEventsPayload;
bool eventPending = false;
bool tapDetected = false;

//smoke 
uint8_t smokeDebounce =0 ; 
bool smokeTriggered = false;
bool smokeTransition = false; 

//Function Prototypes
uint16_t readLTC4015(uint8_t reg);
void tapTask(void *pvParameters);

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
  bool lastReadState[NUM_DCI];
  unsigned long lastDebounceTime[NUM_DCI];

  // Init
  for (int i = 0; i < NUM_DCI; i++) {
    dryContacts[i].state = digitalRead(dryContacts[i].pin);
    lastReadState[i] = dryContacts[i].state;
    lastDebounceTime[i] = millis();
    currentSensorReadings.dciStates[i] = dryContacts[i].state;

    // Init compressed payload state
    _compressedEventsPayload.dciInfo[i].currentState = dryContacts[i].state;
    _compressedEventsPayload.dciInfo[i].lowToHighCount = 0;
    _compressedEventsPayload.dciInfo[i].highToLowCount = 0;
  }

  while (1) {
    for (int i = 0; i < NUM_DCI; i++) {
      bool currentState = digitalRead(dryContacts[i].pin);

      if (currentState != lastReadState[i]) {
        lastDebounceTime[i] = millis();
        lastReadState[i] = currentState;
      }

      if ((millis() - lastDebounceTime[i]) > debounceDelayMs) {
        if (currentState != dryContacts[i].state) {
          bool prevState = dryContacts[i].state;
          dryContacts[i].state = currentState;

          // Update shared readings first before any snapshot
          currentSensorReadings.dciStates[i] = currentState;

          bool isRisingEdge = (prevState == LOW && currentState == HIGH);
          bool isFallingEdge = (prevState == HIGH && currentState == LOW);

          // Track counts in compressedEventsPayload
          if (isRisingEdge) {
            _compressedEventsPayload.dciInfo[i].lowToHighCount++;
          } else if (isFallingEdge) {
            _compressedEventsPayload.dciInfo[i].highToLowCount++;
          }
          _compressedEventsPayload.dciInfo[i].currentState = currentState;

          // Update temp/hum for snapshot
          SensorReadings snapshot = getSensorReadings();
          _compressedEventsPayload.temperature = currentSensorReadings.temperature; //snapshot.temperature;
          _compressedEventsPayload.humidity = currentSensorReadings.humidity;// snapshot.humidity;

          // Serial.println("[EVENT] Compressed DCI Event Payload:");
          // for (int i = 0; i < MAX_DCI; i++) {
          //   Serial.printf("  DCI_%d: State=%s, RISE=%u, FALL=%u\n", i + 1,
          //                 compressedEventsPayload.dciInfo[i].currentState ? "HIGH" : "LOW",
          //                 compressedEventsPayload.dciInfo[i].lowToHighCount,
          //                 compressedEventsPayload.dciInfo[i].highToLowCount);
          // }
          // Serial.printf("  Temperature: %.2f °C\n", compressedEventsPayload.temperature);
          // Serial.printf("     Humidity: %.2f RH\n", compressedEventsPayload.humidity);


          eventPending = true;


          bool hotTriggered = false;

          if (dryContacts[i].isHot && !hotCooldownActive) {
            if ((dryContacts[i].triggerOnHigh && isRisingEdge) || (!dryContacts[i].triggerOnHigh && isFallingEdge)) {
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

          // // Enqueue to Events Queue for all edges
          // EventsPayload evt;
          // SensorReadings snapshot = getSensorReadings();  // now includes updated DCI states
          // memcpy(evt.dciStates, snapshot.dciStates, sizeof(evt.dciStates));
          // evt.temperature = snapshot.temperature;
          // evt.humidity = snapshot.humidity;

          // if (xQueueSend(eventsQueue, &evt, 0) == pdPASS) {
          //   Serial.println("[Event] Enqueued due to DCI edge");
          // } else {
          //   Serial.println("[Event] Queue Full - event not sent");
          // }
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

    // Monitor ADC
    int16_t val_0 = ADS.readADC(0);
    int16_t val_1 = ADS.readADC(1);
    int16_t val_2 = ADS.readADC(2);
    int16_t val_3 = ADS.readADC(3);

    float f = ADS.toVoltage(1);  //  voltage factor

    Serial.print("\tAnalog0: ");
    Serial.print(val_0);
    Serial.print('\t');
    Serial.println(val_0 * f, 3);
    Serial.print("\tAnalog1: ");
    Serial.print(val_1);
    Serial.print('\t');
    Serial.println(val_1 * f, 3);
    Serial.print("\tAnalog2: ");
    Serial.print(val_2);
    Serial.print('\t');
    Serial.println(val_2 * f, 3);
    Serial.print("\tAnalog3: ");
    Serial.print(val_3);
    Serial.print('\t');
    Serial.println(val_3 * f, 3);
    Serial.println();

    int16_t smoke_val = val_0; //random(0, 1023);
    Serial.print("Smoke Val: ");
    Serial.println(smoke_val);
    if(smoke_val >= SMOKE_THRESHOLD)
    {
      smokeDebounce++; 
      Serial.print("Smoke debounce: ");
      Serial.println(smokeDebounce);
    }else if (smoke_val < SMOKE_THRESHOLD){
      smokeDebounce= 0;
      if((!smokeTriggered) && (smokeTransition))
      {
        _compressedEventsPayload.smoke = 0;
        currentSensorReadings.smoke = 0; 
        smokeTriggered = true; 
        smokeTransition = false; 
        Serial.print("No Smoke Detected! \n");
      }
    }

    if((smokeDebounce >= 3) && (!smokeTransition) )
    {
      smokeTransition = true; 
      smokeTriggered = true; 
      smokeDebounce=0; 
      _compressedEventsPayload.smoke = 1; 
      currentSensorReadings.smoke = 1; 
      Serial.print("Smoke Detected! \n");
    }

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
        hb.vibration = snapshot.vibration;
        hb.smoke = snapshot.smoke;


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

void powerMonitorTask(void *pvParameters) {
  while (1) {
    PowerPayload payload;

    payload.vbat = readLTC4015(REG_VBAT) * 192.264 / 1000000.0 * 4.0;
    payload.vin = readLTC4015(REG_VIN) * 1.648 / 1000.0;
    payload.vsys = readLTC4015(REG_VSYS) * 1.648 / 1000.0;
    payload.ibat = readLTC4015(REG_IBAT) * 1.46487 / 1000000.0;
    payload.iin = readLTC4015(REG_IIN) * 1.46487 / 1000000.0;
    payload.DCO2State = digitalRead(DCO_2);

    Serial.printf("[LTC4015] VBAT: %.2fV | VIN: %.2fV | VSYS: %.2fV | IBAT: %.3fA | IIN: %.3fA | DCO2: %d \n",
                  payload.vbat, payload.vin, payload.vsys, payload.ibat, payload.iin, payload.DCO2State);

    if (xQueueSend(powerPayloadQueue, &payload, 0) == pdPASS) {
      Serial.println("[Power] Enqueued power payload");
    } else {
      Serial.println("[Power] Queue full, skipping enqueue");
    }

    vTaskDelay(pdMS_TO_TICKS(powerMonitorIntervalMs));
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

  // Initialize Accelerometer
  ADS.begin();
  ADS.setGain(0);

  // Initialize Accelerometer
  while (!acce.begin()) {
    Serial.println("Cannot Read Accelerometer");
    delay(1000);
  }
  Serial.print("Accelerometer ID : ");
  Serial.println(acce.getID(), HEX);
  //Chip soft reset
  acce.softReset();

  acce.setRange(DFRobot_LIS2DW12::e2_g);
  acce.setPowerMode(DFRobot_LIS2DW12::eContLowPwrLowNoise1_12bit);
  acce.setDataRate(DFRobot_LIS2DW12::eRate_800hz);

  acce.enableTapDetectionOnZ(true);
  acce.enableTapDetectionOnY(true);
  acce.enableTapDetectionOnX(true);

  acce.setTapThresholdOnX(0.5);
  acce.setTapThresholdOnY(0.5);
  acce.setTapThresholdOnZ(0.5);


  acce.setTapDur(/*dur=*/6);
  acce.setTapMode(DFRobot_LIS2DW12::eOnlySingle);
  acce.setInt1Event(DFRobot_LIS2DW12::eDoubleTap);

  xTaskCreatePinnedToCore(
    tapTask,    // Function
    "TapTask",  // Task name
    2048,       // Stack size
    NULL,       // Parameters
    1,          // Priority
    NULL,       // Task handle
    0           // Run on core 1
  );

  hotAlarmTimer = xTimerCreate(
    "HotAlarmTimer",
    pdMS_TO_TICKS(hotAlarmDurationMs),
    pdFALSE,
    NULL,
    [](TimerHandle_t xTimer) {
      Serial.println("[HOT] Timer expired — ALARM would trigger here.");
      digitalWrite(DCO_1, RELAY_ON);
      enqueueTheftAlarm(HATCH_OPEN);

      // Start cooldown AFTER siren fires
      startSirenCooldownTimer();
    });

  hotCooldownTimer = xTimerCreate(
    "HotCooldownTimer",
    pdMS_TO_TICKS(hotCooldownMs),
    pdFALSE,
    NULL,
    [](TimerHandle_t xTimer) {
      hotCooldownActive = false;
      Serial.println("[HOT] Cooldown expired — Hot alarm can now trigger again.");
    });


  // Initialize DCI pins
  for (int i = 0; i < NUM_DCI; i++) {
    pinMode(dryContacts[i].pin, INPUT);
  }

  memset(&compressedEventsPayload, 0, sizeof(compressedEventsPayload));

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
    0);

  enqueueHeartbeatEvery(heartbeatInterval);
}


void stopHotAlarmTimer() {
  if (xTimerIsTimerActive(hotAlarmTimer)) {
    xTimerStop(hotAlarmTimer, 0);
    Serial.println("[HOT] Timer stopped globally.");
  }
  digitalWrite(DCO_1, RELAY_OFF);
}

void startSirenCooldownTimer() {
  if (!xTimerIsTimerActive(hotCooldownTimer)) {
    hotCooldownActive = true;
    xTimerStart(hotCooldownTimer, 0);
    Serial.println("[HOT] Cooldown timer started.");
  }
}


void handleTap() {
  // tap detected
  DFRobot_LIS2DW12::eTap_t tapEvent = acce.tapDetect();
  DFRobot_LIS2DW12::eTapDir_t dir = acce.getTapDirection();

  if (tapEvent == DFRobot_LIS2DW12::eSTap) {
    Serial.print("Single Tap Detected: ");
    _compressedEventsPayload.vibration = SMASHED;
    _compressedEventsPayload.temperature = currentSensorReadings.temperature; //snapshot.temperature;
    _compressedEventsPayload.humidity = currentSensorReadings.humidity;// snapshot.humidity;
    
    tapDetected = true;
  }

  currentSensorReadings.vibration = _compressedEventsPayload.vibration ; 

  if (tapEvent != DFRobot_LIS2DW12::eNoTap) {
    if (dir == DFRobot_LIS2DW12::eDirXUp) {
      //Serial.println("tap detected!");
    } else if (dir == DFRobot_LIS2DW12::eDirXDown) {
      //Serial.println("tap detected!");
    } else if (dir == DFRobot_LIS2DW12::eDirYUp) {
      //Serial.println("tap detected!");
    } else if (dir == DFRobot_LIS2DW12::eDirYDown) {
      //Serial.println("tap detected!");
    } else if (dir == DFRobot_LIS2DW12::eDirZUp) {
      //Serial.println("tap detected!");
    } else if (dir == DFRobot_LIS2DW12::eDirZDown) {
      //Serial.println("tap detected!");
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);  //prevent spamming
  }
}

void tapTask(void *pvParameters) {
  while (1) {
    handleTap();  // Tap detection logic
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
