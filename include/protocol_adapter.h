#ifndef PROTOCOL_ADAPTER_H
#define PROTOCOL_ADAPTER_H

#include <Arduino.h>

String buildReadFrame(uint16_t startReg, uint16_t quantity);
String buildWriteSingleFrame(uint16_t reg, uint16_t value);

// HTTP send
bool httpPostFrame(const char* url, const String& hexFrame, String& outFrameHex);

// Response helpers
bool isModbusException(const String& hex);
uint8_t modbusExceptionCode(const String& hex);
bool decodeFirstRegister_U16(const String& hex, uint16_t& outVal);

bool readRegisterU16(uint16_t reg, uint16_t qty, uint16_t& firstVal);
bool writeSingleRegister(uint16_t reg, uint16_t value);

#endif
