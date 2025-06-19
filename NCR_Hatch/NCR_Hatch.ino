#include <Arduino.h>
#include "LoRaSenderTask.h"
#include "SensorMonitorTask.h"
#include "CLITask.h"
#include "PinConfig.h"
#include "KeypadTask.h"
#include "SystemConfig.h"
#include <Wire.h>
#include "lorawan_handler.h"

#define VERSION_MAJOR     1
#define VERSION_MINOR     1
#define PATCHLEVEL        0
/* Fixed Credentials */
bool isLoRaReady = false;  // Set true after LoRa is initialized

void wdtReset();

// LoRa initialization task
void loraInitTask(void* pvParameters) {
  int err;
  while (!isLoRaReady) {
    loraStat = JOINING;
    err = initLora(appEUI, devEUI, appKEY, devADDR);
    if (err == 0) {
      loraStat = CONNECTED;
      isLoRaReady = true;
      setLoraJoinStatus(isLoRaReady);  // added to start processing Rx Handler
      Serial.println("[LoRa] Initialization complete.");
      setInitialJoinFlag(true);
    } else {
      Serial.println("[LoRa] Init failed, retrying in 5 seconds...");
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
  vTaskDelete(NULL);  // Self-delete task when done
}

void setup() {

  Serial.begin(115200);

  String ver = "Firmware Version: " + String(VERSION_MAJOR) + "." + String(VERSION_MINOR) + "." + String(PATCHLEVEL);
  Serial.println(ver);

  pinMode(WDT_DONE, OUTPUT);
  digitalWrite(WDT_DONE, LOW);

  // WDT reset trigger
  registerOkCallback(wdtReset);

  Wire.begin();

  pinMode(DCO_1, OUTPUT);
  pinMode(DCO_2, OUTPUT);
  digitalWrite(DCO_1, RELAY_OFF);
  

  // Load config before sensor/tasks
  loadConfig();  // Includes intervals, passkey, HOT config,

  bool dco2InitialState = loadDCO2State();
  digitalWrite(DCO_2, dco2InitialState ? RELAY_ON : RELAY_OFF);

  // Start non-blocking LoRa initialization

  initLoraSerial();  // moved it outside the loraInitTask to stop reinitializing the serial port
  xTaskCreatePinnedToCore(
    loraInitTask,
    "LoRaInitTask",
    4096,
    NULL,
    5,
    NULL,
    1);

  createLoRaQueues();
  createSensorTasks();
  createLoRaSenderTask();
  createKeypadTask();
  startCLITask();

  xTaskCreatePinnedToCore(
    loraRxTask,
    "LoRa RX Task",
    4096,  // changed for testing.
    NULL,
    7,
    NULL,
    1);
  xTaskCreatePinnedToCore(
    loraTxTask,
    "LoRa TX Task",
    4096,  // changed for testing.
    NULL,
    7,
    NULL,
    1);

  Serial.println("System Initialized");
}

void loop() {
  vTaskDelay(1);
}

void wdtReset() {
  Serial.println("WDT Reset");
  digitalWrite(WDT_DONE, HIGH);
  vTaskDelay(pdMS_TO_TICKS(100));
  digitalWrite(WDT_DONE, LOW);
}
