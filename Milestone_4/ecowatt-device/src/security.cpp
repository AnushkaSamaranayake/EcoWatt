#include "security.h"
#include <EEPROM.h>
#include <ArduinoJson.h>

// Demo PSK - override in main.cpp or define as const elsewhere
#ifndef DEVICE_PSK
const char* DEVICE_PSK = "demo_psk_change_me";
#endif

#define EEPROM_SIZE 1024
#define NONCE_ADDR 128

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

// ------------------ HMAC Placeholder ------------------
// IMPORTANT: Replace compute_hmac_hex with a real HMAC-SHA256 implementation
// (BearSSL br_hmac_* or ArduinoCrypto). Below is a non-secure placeholder.

static String to_hex(const uint8_t* p, size_t len) {
  String s;
  s.reserve(len*2);
  char buf[3];
  for (size_t i=0;i<len;i++){
    sprintf(buf, "%02x", p[i]);
    s += buf;
  }
  return s;
}

String compute_hmac_hex(const uint8_t* psk, size_t psk_len, const uint8_t* msg, size_t msg_len) {
  // Placeholder: simple checksum (NOT secure). Replace with HMAC-SHA256.
  uint8_t tag[32];
  uint32_t acc = 0;
  for (size_t i=0;i<msg_len;i++) acc += msg[i];
  for (int i=0;i<32;i++) tag[i] = (uint8_t)((acc + i) & 0xFF);
  return to_hex(tag, 32);
}

bool verify_hmac_hex(const char* mac_hex, const uint8_t* psk, size_t psk_len, const uint8_t* msg, size_t msg_len) {
  String expected = compute_hmac_hex(psk, psk_len, msg, msg_len);
  String provided(mac_hex);
  return expected.equalsIgnoreCase(provided);
}

String build_envelope_json(uint32_t nonce, const String& payload_b64, const String& mac_hex) {
  String s = "{";
  s += "\"nonce\":";
  s += String(nonce);
  s += ",\"payload\":\"";
  s += payload_b64;
  s += "\",\"mac\":\"";
  s += mac_hex;
  s += "\"}";
  return s;
}

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
