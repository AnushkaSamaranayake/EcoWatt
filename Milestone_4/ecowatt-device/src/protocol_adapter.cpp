#include "protocol_adapter.h"
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "security.h"

// Config (defined in main)
extern const char* URL_READ;
extern const char* URL_WRITE;
extern const char* API_KEY;


// Slave ID
const uint8_t SLAVE_ID = 0x11;

// --- CRC16 Modbus ---
uint16_t crc16_modbus(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) { crc >>= 1; crc ^= 0xA001; }
      else crc >>= 1;
    }
  }
  return crc;
}

// --- Helper ---
String toHex2(uint8_t b) {
  char buf[3];
  sprintf(buf, "%02X", b);
  return String(buf);
}

// --- Build Frames ---
String buildReadFrame(uint16_t startReg, uint16_t quantity) {
  uint8_t frame[8];
  frame[0] = SLAVE_ID; frame[1] = 0x03;
  frame[2] = (startReg >> 8) & 0xFF;
  frame[3] = (startReg) & 0xFF;
  frame[4] = (quantity >> 8) & 0xFF;
  frame[5] = (quantity) & 0xFF;
  uint16_t crc = crc16_modbus(frame, 6);
  frame[6] = crc & 0xFF;
  frame[7] = (crc >> 8) & 0xFF;

  String hex = "";
  for (int i = 0; i < 8; i++) hex += toHex2(frame[i]);
  return hex;
}

String buildWriteSingleFrame(uint16_t reg, uint16_t value) {
  uint8_t frame[8];
  frame[0] = SLAVE_ID; frame[1] = 0x06;
  frame[2] = (reg >> 8) & 0xFF;
  frame[3] = (reg) & 0xFF;
  frame[4] = (value >> 8) & 0xFF;
  frame[5] = (value) & 0xFF;
  uint16_t crc = crc16_modbus(frame, 6);
  frame[6] = crc & 0xFF;
  frame[7] = (crc >> 8) & 0xFF;

  String hex = "";
  for (int i = 0; i < 8; i++) hex += toHex2(frame[i]);
  return hex;
}

// --- HTTP Utility ---
String extractFrameField(const String& json) {
  int pos = json.indexOf("\"frame\"");
  if (pos < 0) return "";
  int colon = json.indexOf(':', pos);
  int q1 = json.indexOf('"', colon + 1);
  int q2 = json.indexOf('"', q1 + 1);
  return (q1 >= 0 && q2 > q1) ? json.substring(q1 + 1, q2) : "";
}

bool httpPostFrame(const char* url, const String& hexFrame, String& outFrameHex) {
  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, url)) return false;

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + API_KEY);

  String body = String("{\"frame\":\"") + hexFrame + "\"}";
  int code = http.POST(body);
  if (code != 200) { http.end(); return false; }

  String resp = http.getString();
  http.end();
  outFrameHex = extractFrameField(resp);
  return (outFrameHex.length() > 0);
}

// Helpers
bool isModbusException(const String& hex) {
  if (hex.length() < 6) return false;
  uint8_t func = strtoul(hex.substring(2, 4).c_str(), nullptr, 16);
  return func >= 0x80;
}
uint8_t modbusExceptionCode(const String& hex) {
  if (hex.length() < 6) return 0;
  return strtoul(hex.substring(4, 6).c_str(), nullptr, 16);
}
bool decodeFirstRegister_U16(const String& hex, uint16_t& outVal) {
  if (hex.length() < 14) return false;
  String hiLo = hex.substring(6, 10);
  outVal = strtoul(hiLo.c_str(), nullptr, 16);
  return true;
}


// --- High-Level ---
bool readRegisterU16(uint16_t reg, uint16_t qty, uint16_t& firstVal) {
  String tx = buildReadFrame(reg, qty);
  // 🔹 Print the frame we are sending
  Serial.print(F("Read Frame -> "));
  Serial.println(tx);
  String rx;

  // 🔹 Print the frame we received

  if (!httpPostFrame(URL_READ, tx, rx)) return false;
  if (isModbusException(rx)) return false;
  Serial.print(F("Read Resp  <- "));
  Serial.println(rx);
  return decodeFirstRegister_U16(rx, firstVal);
}

bool writeSingleRegister(uint16_t reg, uint16_t value) {
  String tx = buildWriteSingleFrame(reg, value);
  String rx;
  if (!httpPostFrame(URL_WRITE, tx, rx)) return false;
  if (isModbusException(rx)) return false;
  return true;
}
