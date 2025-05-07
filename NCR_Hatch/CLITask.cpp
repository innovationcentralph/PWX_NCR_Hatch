#include "CLITask.h"
#include "SensorMonitorTask.h"
#include "KeypadTask.h"
#include "SystemConfig.h"

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

  else if (command.startsWith("AT+SET_LORA_INTERVAL=")) {
    String valueStr = command.substring(String("AT+SET_LORA_INTERVAL=").length());
    uint32_t newInterval = valueStr.toInt();
    if (newInterval >= 1000 && newInterval <= 60000) {
      saveSendInterval(newInterval);
      Serial.printf("[CLI] Send interval set to %lu ms\n", newInterval);
    } else {
      Serial.println("[CLI] Invalid value. Must be 1000–60000 ms.");
    }
  }

  else if (command.equalsIgnoreCase("AT+GET_LORA_INTERVAL")) {
    Serial.printf("[CLI] Current send interval: %lu ms\n", loraSendInterval);
  }

  else if (command.startsWith("AT+SET_HEARTBEAT_INTERVAL=")) {
    String valueStr = command.substring(String("AT+SET_HEARTBEAT_INTERVAL=").length());
    uint32_t newInterval = valueStr.toInt();
    if (newInterval >= 5000 && newInterval <= 1800000) {  // 5 sec to 30 mins
      saveHeartbeatInterval(newInterval);
      Serial.printf("[CLI] Heartbeat interval set to %lu ms. Restart to take effect \n", newInterval);
    } else {
      Serial.println("[CLI] Invalid value. Must be 5000–1800000 ms (5 sec to 30 mins).");
    }
  }

  else if (command.equalsIgnoreCase("AT+GET_HEARTBEAT_INTERVAL")) {
    Serial.printf("[CLI] Current heartbeat interval: %lu ms\n", heartbeatInterval);
  }

  else if (command.startsWith("AT+SET_PASSKEY=")) {
    String value = command.substring(strlen("AT+SET_PASSKEY="));
    if (value.length() >= 4 && value.length() <= 8) {
      savePasskey(value.c_str());
      Serial.println("[CLI] Passkey updated.");
    } else {
      Serial.println("[CLI] Invalid passkey. Must be 4–8 characters.");
    }
  }

  else if (command.equalsIgnoreCase("AT+GET_PASSKEY")) {
    Serial.printf("[CLI] Current passkey: %s\n", passkey);
  }

  else if (command.startsWith("AT+SET_HOT=")) {
    String payload = command.substring(strlen("AT+SET_HOT="));
    payload.trim();

    int index = 0;
    int lastPos = 0;

    for (int i = 0; i < payload.length() && index < 6; i++) {
      if (payload.charAt(i) == ',' || i == payload.length() - 1) {
        int endPos = (payload.charAt(i) == ',') ? i : i + 1;
        String valStr = payload.substring(lastPos, endPos);
        valStr.trim();
        dryContacts[index].isHot = (valStr.toInt() == 1);
        index++;
        lastPos = i + 1;
      }
    }

    saveHotConfig();  // <--- Save to EEPROM
    Serial.println("[CLI] HOT contact configuration updated.");
  }

  else if (command.equalsIgnoreCase("AT+GET_HOT")) {
    Serial.print("[CLI] HOT flags: ");
    for (int i = 0; i < 6; i++) {
      Serial.print(dryContacts[i].isHot ? "1" : "0");
      if (i < 5) Serial.print(",");
    }
    Serial.println();
  }

  else if (command.startsWith("AT+SET_HOT_TIMEOUT=")) {
    String valueStr = command.substring(strlen("AT+SET_HOT_TIMEOUT="));
    uint32_t val = valueStr.toInt();
    if (val >= 1000 && val <= 60000) {
      saveHotTimeout(val);
      Serial.printf("[CLI] HOT alarm timeout set to %lu ms\n", val);
    } else {
      Serial.println("[CLI] Invalid value. Must be 1000–60000 ms.");
    }
  }

  else if (command.equalsIgnoreCase("AT+GET_HOT_TIMEOUT")) {
    Serial.printf("[CLI] Current HOT alarm timeout: %lu ms\n", hotAlarmDurationMs);
  }

  else if (command.startsWith("AT+SET_DEVEUI=")) {
    String hexStr = command.substring(strlen("AT+SET_DEVEUI="));
    hexStr.trim();
    if (hexStr.length() != 16) {
      Serial.println("[CLI] Invalid length. Must be 16 hex characters.");
      return;
    }

    uint8_t newEUI[8];
    for (int i = 0; i < 8; i++) {
      String byteStr = hexStr.substring(i * 2, i * 2 + 2);
      newEUI[i] = strtoul(byteStr.c_str(), NULL, 16);
    }

    saveDevEUI(newEUI);
    Serial.println("[CLI] devEUI updated. Restart to take effect.");
  }

  else if (command.equalsIgnoreCase("AT+GET_DEVEUI")) {
    Serial.print("[CLI] devEUI = ");
    for (int i = 0; i < 8; i++) {
      Serial.printf("%02X", devEUI[i]);
    }
    Serial.println();
  }

  else if (command.startsWith("AT+SET_APPEUI=")) {
    String hexStr = command.substring(strlen("AT+SET_APPEUI="));
    hexStr.trim();

    if (hexStr.length() != 16) {
      Serial.println("[CLI] Invalid length. AppEUI must be 16 hex characters (8 bytes).");
      return;
    }

    uint8_t newAppEUI[8];
    for (int i = 0; i < 8; i++) {
      String byteStr = hexStr.substring(i * 2, i * 2 + 2);
      newAppEUI[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
    }

    saveAppEUI(newAppEUI);
    Serial.print("[CLI] AppEUI updated to: ");
    for (int i = 0; i < 8; i++) {
      Serial.printf("%02X", appEUI[i]);
    }
    Serial.println();
  }

  else if (command.equalsIgnoreCase("AT+GET_APPEUI")) {
    Serial.print("[CLI] Current AppEUI: ");
    for (int i = 0; i < 8; i++) {
      Serial.printf("%02X", appEUI[i]);
    }
    Serial.println();
  }

  else if (command.startsWith("AT+SET_APPKEY=")) {
    String hexStr = command.substring(strlen("AT+SET_APPKEY="));
    hexStr.trim();

    if (hexStr.length() != 32) {
      Serial.println("[CLI] Invalid length. AppKey must be 32 hex characters (16 bytes).");
      return;
    }

    uint8_t newAppKey[16];
    for (int i = 0; i < 16; i++) {
      String byteStr = hexStr.substring(i * 2, i * 2 + 2);
      newAppKey[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
    }

    saveAppKEY(newAppKey);
    Serial.print("[CLI] AppKey updated to: ");
    for (int i = 0; i < 16; i++) {
      Serial.printf("%02X", appKEY[i]);
    }
    Serial.println();
  }

  else if (command.equalsIgnoreCase("AT+GET_APPKEY")) {
    Serial.print("[CLI] Current AppKey: ");
    for (int i = 0; i < 16; i++) {
      Serial.printf("%02X", appKEY[i]);
    }
    Serial.println();
  }

  else if (command.startsWith("AT+SET_DCI_EDGE=")) {
    String payload = command.substring(strlen("AT+SET_DCI_EDGE="));
    payload.trim();

    int index = 0;
    int lastPos = 0;
    for (int i = 0; i < payload.length() && index < 6; i++) {
      if (payload.charAt(i) == ',' || i == payload.length() - 1) {
        int endPos = (payload.charAt(i) == ',') ? i : i + 1;
        String valStr = payload.substring(lastPos, endPos);
        dryContacts[index].triggerOnHigh = (valStr.toInt() == 1);
        index++;
        lastPos = i + 1;
      }
    }

    saveTriggerEdgeConfig();
    Serial.println("[CLI] DCI edge configuration saved.");
  }

  else if (command.equalsIgnoreCase("AT+GET_DCI_EDGE")) {
    Serial.print("[CLI] Trigger edges: ");
    for (int i = 0; i < 6; i++) {
      Serial.print(dryContacts[i].triggerOnHigh ? "1" : "0");
      if (i < 5) Serial.print(",");
    }
    Serial.println();
  }






  else if (command.equalsIgnoreCase("AT+HELP") || command.equalsIgnoreCase("?")) {
    Serial.println(F("\n--- Available CLI Commands ---"));
    Serial.println(F("AT+HELP or ?                    - Show this help menu"));
    Serial.println(F("AT+STOPALARM                   - Stop the hot alarm timer"));
    Serial.println(F("AT+TRIG_S_ALARM                - Trigger silent alarm"));
    Serial.println(F("AT+SET_LORA_INTERVAL=<ms>      - Set LoRa send interval (1000–60000 ms)"));
    Serial.println(F("AT+GET_LORA_INTERVAL           - Get current LoRa send interval"));
    Serial.println(F("AT+SET_HEARTBEAT_INTERVAL=<ms> - Set heartbeat interval (5000–1800000 ms)"));
    Serial.println(F("AT+GET_HEARTBEAT_INTERVAL      - Get current heartbeat interval"));
    Serial.println(F("AT+SET_HOT_TIMEOUT=<ms>        - Set HOT timer duration (1000–60000 ms)"));
    Serial.println(F("AT+GET_HOT_TIMEOUT             - Get current HOT timer duration"));
    Serial.println(F("AT+SET_HOT=1,0,0,1,0,1         - Set HOT status per DCI (1=HOT, 0=normal)"));
    Serial.println(F("AT+GET_HOT                     - Show current HOT DCI flags"));
    Serial.println(F("AT+SET_PASSKEY=<4-8 digit key> - Set passkey"));
    Serial.println(F("AT+GET_PASSKEY                 - Show current passkey"));
    Serial.println(F("AT+SET_DEVEUI=<16 hex chars>    - Set DevEUI (e.g., AT+SET_DEVEUI=1010101007DFF66C)"));
    Serial.println(F("AT+GET_DEVEUI                   - Show current DevEUI"));
    Serial.println(F("AT+SET_APPEUI=<16 HEX>           - Set AppEUI (8 bytes, hex format)"));
    Serial.println(F("AT+GET_APPEUI                   - Show current AppEUI"));
    Serial.println(F("AT+SET_APPKEY=<32 HEX>          - Set AppKey (16 bytes, hex format)"));
    Serial.println(F("AT+GET_APPKEY                   - Show current AppKey"));
    Serial.println(F("AT+SET_DCI_EDGE=1,0,1,1,0,0       - Set edge per DCI (1=HIGH trigger, 0=LOW)"));
    Serial.println(F("AT+GET_DCI_EDGE                   - Show current trigger edges"));
    Serial.println(F("--------------------------------\n"));
  }


  else {
    Serial.println("[CLI] Unknown command.");
  }
}
