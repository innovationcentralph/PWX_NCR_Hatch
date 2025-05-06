#include <Arduino.h>
#include "LoRaSenderTask.h"
#include "SensorMonitorTask.h"
#include "CLITask.h"
#include "PinConfig.h"
#include "KeypadTask.h"
#include "SystemConfig.h"
#include <Wire.h>
#include "lorawan_handler.h"

/* Fixed Credentials */
uint8_t appEUI[8]= {0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10}; 
uint8_t devEUI[8]= {0x10,0x10,0x10,0x10,0x01,0xF9,0x42,0x1B}; 
uint8_t appKEY[16]= {0x8A,0xD7,0x3D,0x3D,0x3B,0x47,0xD4,0x3E,0xF5,0xB2,0xF8,0x25,0x26,0xAA,0xFB,0x50}; 
uint8_t devADDR[4]= {0xE0, 0x24, 0x4F, 0x52};


bool isLoRaReady = false;  // Set true after LoRa is initialized

void wdtReset();

// LoRa initialization task
void loraInitTask(void* pvParameters) {
  int err;
  while (!isLoRaReady) {
    err = initLora(appEUI, devEUI, appKEY, devADDR);
    if (err == 0) {
      isLoRaReady = true;
      setLoraJoinStatus(isLoRaReady); // added to start processing Rx Handler 
      Serial.println("[LoRa] Initialization complete.");
    } else {
      Serial.println("[LoRa] Init failed, retrying in 5 seconds...");
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
  vTaskDelete(NULL);  // Self-delete task when done
}

void setup() {
  Serial.begin(115200);

  pinMode(WDT_DONE, OUTPUT);
  digitalWrite(WDT_DONE, LOW);

  // WDT reset trigger
  registerOkCallback(wdtReset);

  Wire.begin();

  pinMode(DCO_1, OUTPUT);
  digitalWrite(DCO_1, RELAY_OFF);

  // Load config before sensor/tasks
  loadConfig();  // Includes intervals, passkey, HOT config, 

  // Start non-blocking LoRa initialization
  
  initLoraSerial(); // moved it outside the loraInitTask to stop reinitializing the serial port
  xTaskCreatePinnedToCore(
    loraInitTask,
    "LoRaInitTask",
    4096,
    NULL,
    5,
    NULL,
    1
  );

  createLoRaQueues();
  createSensorTasks();
  createLoRaSenderTask();
  // createKeypadTask();
  startCLITask();

  xTaskCreatePinnedToCore(
    loraRxTask,
    "LoRa RX Task",
    4096, // changed for testing. 
    NULL,
    7,
    NULL,
    1
  );

  Serial.println("System Initialized");
}

void loop() {
  vTaskDelay(1);
}

void wdtReset() {
  digitalWrite(WDT_DONE, HIGH);
  vTaskDelay(pdMS_TO_TICKS(100));
  digitalWrite(WDT_DONE, LOW);
}
