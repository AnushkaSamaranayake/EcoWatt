#pragma once
#include <EEPROM.h>
#include <WString.h>

#define EEPROM_SIZE 128
#define VERSION_ADDR 0
#define VERSION_MAX_LEN 16

inline void version_init() {
  EEPROM.begin(EEPROM_SIZE);
}

inline void saveCurrentVersion(const char* version) {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < VERSION_MAX_LEN; ++i) {
    EEPROM.write(VERSION_ADDR + i, version[i]);
    if (version[i] == '\0') break;
  }
  EEPROM.commit();
}

inline String loadCurrentVersion() {
  EEPROM.begin(EEPROM_SIZE);
  char buf[VERSION_MAX_LEN];
  for (int i = 0; i < VERSION_MAX_LEN; ++i)
    buf[i] = EEPROM.read(VERSION_ADDR + i);
  buf[VERSION_MAX_LEN - 1] = '\0';
  return String(buf);
}
