#include "protocol_adapter.h"
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

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

// Convert hex string to byte array
bool hexToBytes(const String& hex, std::vector<uint8_t>& out) {
  if (hex.length() % 2 !=0) return false;
  out.clear();
  for (size_t i = 0; i < hex.length(); i += 2) {
    out.push_back(strtoul(hex.substring(i,i+2).c_str(), nullptr, 16));
  }
  return true;
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

// ---- Error handling and Decoding ---
String modbusExceptionMessage(uint8_t code) {
  switch (code) {
    case 0x01: return F("Illegal Function");
    case 0x02: return F("Illegal Data Address");
    case 0x03: return F("Illegal Data Value");
    case 0x04: return F("Slave Device Failure");
    case 0x05: return F("Acknowledge (processing delayed)");
    case 0x06: return F("Slave Device Busy");
    case 0x08: return F("Memory Parity Error");
    case 0x0A: return F("Gateway Path Unavailable");
    case 0x0B: return F("Gateway Target Failed to Respond");
    default:   return F("Unknown Exception");
  }
}

bool isModbusException(const String& hex) {
  if (hex.length() < 6) return false;
  uint8_t func = strtoul(hex.substring(2, 4).c_str(), nullptr, 16);
  return func >= 0x80;
}
uint8_t modbusExceptionCode(const String& hex) {
  if (hex.length() < 6) return 0;
  return strtoul(hex.substring(4, 6).c_str(), nullptr, 16);
}

//---- Validation -----
bool validateCRC(const String& hex) {
  std::vector<uint8_t> bytes;
  if (!hexToBytes(hex, bytes) || bytes.size() < 4) return false;
  uint16_t crcCalc = crc16_modbus(bytes.data(), bytes.size() - 2);
  uint16_t crcRecv = bytes[bytes.size()-2] | (bytes[bytes.size()-1] << 8);
  return (crcCalc == crcRecv);
}

// --- Decode ---
bool decodeRegisters_U16(const String& hex, std::vector<uint16_t>& outVals) {
  outVals.clear();
  if (!validateCRC(hex)) {
    Serial.println(F("Invalid CRC"));
    return false;
  }
  std::vector<uint8_t> bytes;
  hexToBytes(hex, bytes);
  if (bytes.size() < 5) return false;

  uint8_t byteCount = bytes[2];
  if (bytes.size() < 3 + byteCount + 2) return false; // header + data + CRC

  for (int i = 0; i< byteCount + 2; i++) {
    uint16_t val = (bytes[3 + i] << 8) | bytes[4 + i];
    outVals.push_back(val);
  }
  return true;
}


// --- High-Level ---
bool readRegisterU16(uint16_t reg, uint16_t qty, std::vector<uint16_t>& values, int retries) {
  String tx = buildReadFrame(reg, qty);
  Serial.print(F("Read Frame -> ")); Serial.println(tx);

  for (int attempt = 1; attempt <= retries; attempt++) {
    String rx;
    if (!httpPostFrame(URL_READ, tx, rx)) {
      Serial.printf("HTTP error (attempt %d/%d)\n", attempt, retries);
      delay(200);
      continue;
    }

    if (isModbusException(rx)) {
      uint8_t code = modbusExceptionCode(rx);
      Serial.print(F("Modbus Exception: "));
      Serial.println(modbusExceptionMessage(code));
      return false;
    }

    if (decodeRegisters_U16(rx, values)) {
      Serial.print(F("Read Resp  <- ")); Serial.println(rx);
      return true;
    }

    Serial.printf("Decode/CRC error (attempt %d/%d)\n", attempt, retries);
    delay(200);
  }
  return false;
}

bool writeSingleRegister(uint16_t reg, uint16_t value, int retries) {
  String tx = buildWriteSingleFrame(reg, value);
  for (int attempt = 1; attempt <= retries; attempt++) {
    String rx;
    if (!httpPostFrame(URL_WRITE, tx, rx)) {
      Serial.printf("HTTP error (attempt %d/%d)\n", attempt, retries);
      delay(200);
      continue;
    }
    if (isModbusException(rx)) {
      uint8_t code = modbusExceptionCode(rx);
      Serial.print(F("Modbus Exception: "));
      Serial.println(modbusExceptionMessage(code));
      return false;
    }
    if (validateCRC(rx)) {
      Serial.print(F("Write Resp <- ")); Serial.println(rx);
      return true;
    }
    Serial.printf("CRC error (attempt %d/%d)\n", attempt, retries);
    delay(200);
  }
  return false;
}
