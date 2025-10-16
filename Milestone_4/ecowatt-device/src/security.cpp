#include "security.h"
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <HMAC.h>
#include <SHA256.h>

// Demo PSK - override in main.cpp or define as const elsewhere
#ifndef DEVICE_PSK
const char* DEVICE_PSK = "demo_psk_change_me";
#endif

#define EEPROM_SIZE 1024
#define NONCE_ADDR 128

// ===================================================
// Nonce handling (persistent counter)
// ===================================================
void nonce_init() {
  EEPROM.begin(EEPROM_SIZE);
}

uint32_t nonce_load() {
  uint32_t n = 0;
  EEPROM.get(NONCE_ADDR, n);
  return n;
}

void nonce_save(uint32_t n) {
  EEPROM.put(NONCE_ADDR, n);
  EEPROM.commit();
}

// ===================================================
// Utility: convert binary to hex string
// ===================================================
static String to_hex(const uint8_t* p, size_t len) {
  String s;
  s.reserve(len * 2);
  char buf[3];
  for (size_t i = 0; i < len; i++) {
    sprintf(buf, "%02x", p[i]);
    s += buf;
  }
  return s;
}

// ===================================================
// HMAC placeholder (replace with real implementation if needed)
// ===================================================
String compute_hmac_hex(const uint8_t* psk, size_t psk_len,
                        const uint8_t* msg, size_t msg_len) {
  uint8_t tag[32];
  HMAC<SHA256> hmac;        // uses your HMAC.h + rweather SHA256
  hmac.reset(psk, psk_len);
  hmac.update(msg, msg_len);
  hmac.finalize(tag, sizeof(tag));

  char hex[65];
  for (int i = 0; i < 32; i++) sprintf(hex + i*2, "%02x", tag[i]);
  hex[64] = '\0';
  return String(hex);
}

bool verify_hmac_hex(const char* mac_hex, const uint8_t* psk, size_t psk_len,
                     const uint8_t* msg, size_t msg_len) {
  String expected = compute_hmac_hex(psk, psk_len, msg, msg_len);
  String provided(mac_hex);
  return expected.equalsIgnoreCase(provided);
}

// ===================================================
// FIXED: Safe JSON envelope builder (escapes inner payload)
// ===================================================
String build_envelope_json(uint32_t nonce, const String& inner_payload, const String& mac_hex) {
  DynamicJsonDocument doc(2048);
  doc["nonce"] = nonce;
  doc["payload"] = inner_payload;  // ArduinoJson will handle proper escaping automatically
  doc["mac"] = mac_hex;

  String json;
  serializeJson(doc, json);
  return json;
}

// ===================================================
// Envelope parser (for server responses)
// ===================================================
bool parse_envelope_json(const String& json, SecuredEnvelope& out) {
  StaticJsonDocument<512> doc;
  auto err = deserializeJson(doc, json);
  if (err) return false;
  if (!doc.containsKey("nonce") || !doc.containsKey("payload") || !doc.containsKey("mac")) return false;
  out.nonce = doc["nonce"].as<uint32_t>();
  out.payload = String((const char*)doc["payload"].as<const char*>());
  out.mac = String((const char*)doc["mac"].as<const char*>());
  return true;
}
