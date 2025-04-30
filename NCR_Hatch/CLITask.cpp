#include "CLITask.h"
#include "SensorMonitorTask.h"
#include "KeypadTask.h"

#define CLI_TASK_STACK_SIZE 4096
#define CLI_TASK_PRIORITY 1

static void CLITask(void* pvParameters) {
  Serial.println("[CLI] Task started. Type AT commands:");

  while (true) {
    if (Serial.available()) {
      String input = Serial.readStringUntil('\n');
      input.trim();

      if (!input.isEmpty()) {
        Serial.print("[CLI] Received: ");
        Serial.println(input);
        handleCLICommand(input);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void startCLITask() {
  xTaskCreatePinnedToCore(
    CLITask,
    "CLITask",
    CLI_TASK_STACK_SIZE,
    NULL,
    CLI_TASK_PRIORITY,
    NULL,
    1  // Run on core 1
  );
}

void handleCLICommand(const String& command) {

  if (command.equalsIgnoreCase("AT+STOPALARM")) {
    Serial.println("[CLI] Stopping Hot Alarm Timer...");
    stopHotAlarmTimer();  
  }

  else if (command.equalsIgnoreCase("AT+TRIG_S_ALARM")) {
    Serial.println("[CLI] Triggering Silent Alarm...");
    triggerSilentAlarm();
  }

  else {
    Serial.println("[CLI] Unknown command.");
  }
  
}
