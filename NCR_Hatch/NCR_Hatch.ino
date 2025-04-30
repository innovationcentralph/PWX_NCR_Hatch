// main.cpp
#include <Arduino.h>
#include "LoRaSenderTask.h"
#include "SensorMonitorTask.h"
#include "CLITask.h"
#include "PinConfig.h"
#include "KeypadTask.h"
#include <Wire.h>

void setup() {
  Serial.begin(115200);

  Wire.begin();  

  pinMode(DCO_1, OUTPUT);
  digitalWrite(DCO_1, RELAY_OFF);

  createLoRaQueues();       
  createSensorTasks();     
  createLoRaSenderTask();
  createKeypadTask();

  startCLITask();    

  Serial.println("System Initialized");
}

void loop() {
  vTaskDelay(1);
}
