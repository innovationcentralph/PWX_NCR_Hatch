// KeypadTask.cpp
#include "KeypadTask.h"
#include <Wire.h>
#include "I2CKeyPad.h"
#include "SensorMonitorTask.h"
#include "SystemConfig.h"  // At the top
#include "LoRaSenderTask.h"
#include <LiquidCrystal_I2C.h>

#define I2C_ADDR 0x27
I2CKeyPad keyPad(I2C_ADDR);
LiquidCrystal_I2C lcd(0x26, 16, 2);

static char myKeys[16] = {
  '1', '2', '3', 0,
  '4', '5', '6', 0,
  '7', '8', '9', 0,
  '*', '0', '#', 0
};

// static const char correctPassword[] = "123456";
static char enteredPassword[9];
static uint8_t inputIndex = 0;
static uint8_t lastKey = I2C_KEYPAD_NOKEY;

void triggerSilentAlarm();
void enqueuePasskeyPayload(const char* entered, uint8_t length, bool isCorrect, bool isSilent);

void keypadTask(void* pvParameters) {
  Wire.begin();
  keyPad.begin();
  keyPad.setKeyPadMode(I2C_KEYPAD_4x4);
  keyPad.loadKeyMap(myKeys);

  lcd.init();
  lcd.backlight();
  lcd.setCursor((16 - strlen("ENTER PASSKEY")) / 2, 0);
  lcd.print("ENTER PASSKEY");
  lcd.setCursor(0, 1);
  lcd.print("                ");  // clear

  while (1) {
    uint8_t currentKey = keyPad.getKey();

    if (currentKey == I2C_KEYPAD_NOKEY) {
      if (lastKey != I2C_KEYPAD_NOKEY && myKeys[lastKey] != 0) {
        char key = myKeys[lastKey];

        if (key == '*') {
          inputIndex = 0;
          memset(enteredPassword, 0, sizeof(enteredPassword));
          Serial.println("[Clear] Input cleared.");
          lcd.setCursor(0, 1);
          lcd.print("                ");  // clear line
        } else if (key == '#') {
          enteredPassword[inputIndex] = '\0';

          bool isCorrect = false;
          bool isSilent = false;

          if (strncmp(enteredPassword, passkey, strlen(passkey)) == 0) {
            if (strlen(enteredPassword) == strlen(passkey)) {
              isCorrect = true;
              Serial.println("[Success] Password correct.");
              stopHotAlarmTimer();
            } else if (enteredPassword[strlen(passkey)] == '0') {
              isCorrect = true;
              isSilent = true;
              stopHotAlarmTimer();
              triggerSilentAlarm();
            }
          } else {
            Serial.print("[Failed] You entered: ");
            Serial.println(enteredPassword);
          }

          enqueuePasskeyPayload(enteredPassword, inputIndex, isCorrect, isSilent);

          // Show result on LCD
          lcd.setCursor(0, 1);
          lcd.print("                ");
          if (isCorrect) {
            lcd.setCursor((16 - strlen("Correct")) / 2, 1);
            lcd.print("Correct");
          } else {
            lcd.setCursor((16 - strlen("Wrong!")) / 2, 1);
            lcd.print("Wrong!");
          }

          vTaskDelay(pdMS_TO_TICKS(1500));  // show result for 1.5s
          lcd.setCursor(0, 1);
          lcd.print("                ");  // clear
          inputIndex = 0;

          inputIndex = 0;
          memset(enteredPassword, 0, sizeof(enteredPassword));

        } else if (inputIndex < sizeof(enteredPassword) - 1) {
          enteredPassword[inputIndex++] = key;
          Serial.print("Key Entered: ");
          Serial.println(key);

          lcd.setCursor(0, 1);
          lcd.print("                ");
          lcd.setCursor((16 - inputIndex) / 2, 1);
          for (uint8_t i = 0; i < inputIndex; i++) {
            lcd.print("*");
          }

        } else {
          Serial.println("[Error] Too many digits. Press * to clear.");
        }

        lastKey = I2C_KEYPAD_NOKEY;
      }
    } else {
      lastKey = currentKey;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void createKeypadTask() {
  xTaskCreatePinnedToCore(
    keypadTask,
    "KeypadTask",
    4096,  //2048,
    NULL,
    1,
    NULL,
    1);
}

void triggerSilentAlarm() {
  Serial.println("[Success] Password correct.");
  Serial.println("[ALARM] Silent alarm triggered!");

  enqueueTheftAlarm(KEY_SILENT_ALARM);
}





void enqueuePasskeyPayload(const char* entered, uint8_t length, bool isCorrect, bool isSilent) {
  PassKeyPayload pkPayload;
  memset(&pkPayload, 0, sizeof(pkPayload));

  pkPayload.len = length;
  strncpy(pkPayload.passk, entered, sizeof(pkPayload.passk) - 1);
  pkPayload.passk[sizeof(pkPayload.passk) - 1] = '\0';

  pkPayload.passkeyStat = isCorrect ? CORRECT_PASSKEY : INCORRECT_PASSKEY;
  pkPayload.passkeyType = isSilent ? SILENT : NOT_SILENT;

  if (xQueueSend(passKeyQueue, &pkPayload, 0) == pdPASS) {
    Serial.println("[Keypad] PassKeyPayload enqueued.");
  } else {
    Serial.println("[Keypad] PassKeyQueue Full - could not enqueue.");
  }
}
