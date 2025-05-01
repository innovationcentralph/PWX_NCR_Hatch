#pragma once
#include <EEPROM.h>

#define EEPROM_ADDR  0x50  // I2C address of external EEPROM 
#define LTC4015_ADDR 0x68  // I2C address for LTC4015

// LTC4015 Registers
#define REG_VBAT 0x3A
#define REG_VIN  0x3B
#define REG_VSYS 0x3C
#define REG_IBAT 0x3D
#define REG_IIN  0x3E


#define NUM_DCI 6 // NUmber of Dry Contact Inputs


// struct DryContactInput {
//   uint8_t pin;
//   bool state;
//   bool isHot;  // true if this DCI should be closely monitored
// };


#define EEPROM_SEND_INTERVAL_ADDR      0 
#define EEPROM_HEARTBEAT_INTERVAL_ADDR 4 
#define EEPROM_PASSKEY_ADDR            8
#define EEPROM_HOT_FLAGS_ADDR          20  
#define EEPROM_HOT_TIMEOUT_ADDR        24 

// Default value if EEPROM is empty
#define DEFAULT_SEND_INTERVAL_MS       6000
#define DEFAULT_HEARTBEAT_INTERVAL_MS  15000
#define DEFAULT_PASSKEY                "123456"
#define DEFAULT_HOT_TIMEOUT_MS         10000

extern uint32_t loraSendInterval;
extern uint32_t heartbeatInterval;
extern uint32_t hotAlarmDurationMs;
extern char passkey[9];

void loadConfig();
void saveSendInterval(uint32_t interval);
void saveHeartbeatInterval(uint32_t interval);
void savePasskey(const char* newPasskey);
void saveHotConfig();
void loadHotConfig();
void saveHotTimeout(uint32_t value);

