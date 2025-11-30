

#include <Arduino.h>
#include <stdint.h>

#define PSK_MAX_LEN 64

// PSK: for demo we define in main.cpp; real projects store more securely.
extern const char* DEVICE_PSK;

// Envelope struct
struct SecuredEnvelope {
  uint32_t nonce;
  String payload; // base64 or JSON
  String mac;     // hex
};

// HMAC wrapper - compute hex string of HMAC-SHA256
String compute_hmac_hex(const uint8_t* psk, size_t psk_len, const uint8_t* msg, size_t msg_len);
bool verify_hmac_hex(const char* mac_hex, const uint8_t* psk, size_t psk_len, const uint8_t* msg, size_t msg_len);

// nonce persistence
void nonce_init();
uint32_t nonce_load();
void nonce_save(uint32_t n);

// helpers for JSON envelope
String build_envelope_json(uint32_t nonce, const String& payload_b64, const String& mac_hex);
bool parse_envelope_json(const String& json, SecuredEnvelope& out);
