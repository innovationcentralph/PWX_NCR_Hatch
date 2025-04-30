#define EEPROM_ADDR  0x50  // I2C address of external EEPROM 
#define LTC4015_ADDR 0x68  // I2C address for LTC4015

// LTC4015 Registers
#define REG_VBAT 0x3A
#define REG_VIN  0x3B
#define REG_VSYS 0x3C
#define REG_IBAT 0x3D
#define REG_IIN  0x3E


#define NUM_DCI 6 // NUmber of Dry Contact Inputs


struct DryContactInput {
  uint8_t pin;
  bool state;
  bool isHot;  // true if this DCI should be closely monitored
};