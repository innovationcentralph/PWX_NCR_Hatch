// main.cpp
#include <Arduino.h>
#include "LoRaSenderTask.h"
#include "SensorMonitorTask.h"
#include "CLITask.h"
#include <Wire.h>

void setup() {
  Serial.begin(115200);

  Wire.begin();  

  createLoRaQueues();       
  createSensorTasks();     
  createLoRaSenderTask();

  startCLITask();    

  Serial.println("System Initialized");
}

void loop() {
  vTaskDelay(1);
}
