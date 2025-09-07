#ifndef PROTOCOL_ADAPTER_H
#define PROTOCOL_ADAPTER_H

#include <Arduino.h>

// API URLs and credentials
extern const char* URL_READ;
extern const char* URL_WRITE;
extern const char* API_KEY;

String buildReadFrame(uint16_t startReg, uint16_t quantity);
String buildWriteSingleFrame(uint16_t reg, uint16_t value);

// HTTP send
bool httpPostFrame(const char* url, const String& hexFrame, String& outFrameHex);

// Modbus error handling
bool isModbusException(const String& hex);
uint8_t modbusExceptionCode(const String& hex);
String modbusExceptionMessage(uint8_t code);

//Validation and decoding
bool validateCRC(const String& hex);
bool decodeRegisters_U16(const String& hex, std::vector<uint16_t>& outVals);

// High-level read/write
bool readRegisterU16(uint16_t reg, uint16_t qty, std::vector<uint16_t>& values, int retries=3);
bool writeSingleRegister(uint16_t reg, uint16_t value, int retries=3);

#endif
