// SensorMonitorTask.h
#pragma once

#include <Arduino.h>

// Dry Contact Pins (define or include from your pin config)
#define DCI_1 33
#define DCI_2 32
#define DCI_3 35
#define DCI_4 34
#define DCI_5 39
#define DCI_6 36

struct DryContactInput {
  uint8_t pin;
  bool state;
  bool isHot;
};

struct SensorReadings {
  float temperature;
  float humidity;
  bool dciStates[6];
};

extern DryContactInput dryContacts[6];

void monitorDryContactsTask(void *pvParameters);
void monitorSHTSensorTask(void *pvParameters);
void createSensorTasks();
SensorReadings getSensorReadings();

void stopHotAlarmTimer();


enum TheftAlarmType {
  HATCH_OPEN,
  SILENT_ALARM
};

void enqueueTheftAlarm(TheftAlarmType type);  // Updated declaration
