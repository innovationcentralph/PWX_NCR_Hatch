// main.cpp
#include <Arduino.h>
#include "LoRaSenderTask.h"
#include "SensorMonitorTask.h"
#include "CLITask.h"
#include "PinConfig.h"
#include "KeypadTask.h"
#include "SystemConfig.h"
#include <Wire.h>
#include "lorawan_handler.h"

uint8_t appEUI[8] = { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10 };
uint8_t devEUI[8] = { 0x10, 0x10, 0x10, 0x10, 0x0D, 0xC8, 0xFD, 0x5B };
uint8_t appKEY[16] = { 0x8A, 0xD7, 0x3D, 0x3D, 0x3B, 0x47, 0xD4, 0x3E, 0xF5, 0xB2, 0xF8, 0x25, 0x26, 0xAA, 0xD1, 0x06 };
uint8_t devADDR[4] = { 0xE0, 0x24, 0x0A, 0x4F };

void setup() {
  Serial.begin(115200);

  Wire.begin();

  pinMode(DCO_1, OUTPUT);
  digitalWrite(DCO_1, RELAY_OFF);

  // Initialize Lora
  bool initialized = false;
  int err;
  initLoraSerial(); 
  Serial.begin(115200); 
  while(!initialized)
  {

    delay(1000); 
    err = initLora(appEUI, devEUI, appKEY, devADDR); 
    if(!err) initialized = true; 
    delay(2000); 

  }

  loadConfig();  // Sending Interval, HeartBeat Interval, PassKey, DC Properties

  createLoRaQueues();
  createSensorTasks();
  createLoRaSenderTask();
  // createKeypadTask();
  
  startCLITask();
  xTaskCreatePinnedToCore(
      loraRxTask,
      "LoRa RX Task", 
      2048,  
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
