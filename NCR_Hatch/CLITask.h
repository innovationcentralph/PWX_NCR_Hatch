#pragma once

#include <Arduino.h>

// Public function to start CLI task
void startCLITask();

// Link to existing function
extern void stopHotAlarmTimer();  // From SensorManagerTask
void handleCLICommand(const String& command);
