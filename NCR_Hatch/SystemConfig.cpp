#include "SystemConfig.h"
#include "SensorMonitorTask.h" 

uint32_t loraSendInterval = DEFAULT_SEND_INTERVAL_MS;
uint32_t heartbeatInterval = DEFAULT_HEARTBEAT_INTERVAL_MS;
uint32_t hotAlarmDurationMs = DEFAULT_HOT_TIMEOUT_MS;
char passkey[9] = DEFAULT_PASSKEY;

void loadConfig() {
  EEPROM.begin(64);  // allocate space
  EEPROM.get(EEPROM_SEND_INTERVAL_ADDR, loraSendInterval);
  EEPROM.get(EEPROM_HEARTBEAT_INTERVAL_ADDR, heartbeatInterval);
  EEPROM.get(EEPROM_PASSKEY_ADDR, passkey);
  EEPROM.get(EEPROM_HOT_TIMEOUT_ADDR, hotAlarmDurationMs);
  loadHotConfig();

  if (loraSendInterval < 1000 || loraSendInterval > 60000) {
    loraSendInterval = DEFAULT_SEND_INTERVAL_MS;
  }

  if (heartbeatInterval < 5000 || heartbeatInterval > 1800000) {
    heartbeatInterval = DEFAULT_HEARTBEAT_INTERVAL_MS;
  }

  if (strlen(passkey) < 4 || strlen(passkey) > 8) {
    strncpy(passkey, DEFAULT_PASSKEY, sizeof(passkey));
  }

  if (hotAlarmDurationMs < 1000 || hotAlarmDurationMs > 60000){
    hotAlarmDurationMs = DEFAULT_HOT_TIMEOUT_MS;
  }

}

void saveSendInterval(uint32_t interval) {
  loraSendInterval = interval;
  EEPROM.put(EEPROM_SEND_INTERVAL_ADDR, loraSendInterval);
  EEPROM.commit();
}

void saveHeartbeatInterval(uint32_t interval) {
  heartbeatInterval = interval;
  EEPROM.put(EEPROM_HEARTBEAT_INTERVAL_ADDR, heartbeatInterval);
  EEPROM.commit();
}

void savePasskey(const char* newPasskey) {
  strncpy(passkey, newPasskey, sizeof(passkey) - 1);
  passkey[sizeof(passkey) - 1] = '\0';
  EEPROM.put(EEPROM_PASSKEY_ADDR, passkey);
  EEPROM.commit();
}

void saveHotConfig() {
  for (int i = 0; i < 6; i++) {
    EEPROM.write(EEPROM_HOT_FLAGS_ADDR + i, dryContacts[i].isHot ? 1 : 0);
  }
  EEPROM.commit();
}

void loadHotConfig() {
  for (int i = 0; i < 6; i++) {
    uint8_t val = EEPROM.read(EEPROM_HOT_FLAGS_ADDR + i);
    dryContacts[i].isHot = (val == 1);
  }
}

void saveHotTimeout(uint32_t value) {
  hotAlarmDurationMs = value;
  EEPROM.put(EEPROM_HOT_TIMEOUT_ADDR, hotAlarmDurationMs);
  EEPROM.commit();
}