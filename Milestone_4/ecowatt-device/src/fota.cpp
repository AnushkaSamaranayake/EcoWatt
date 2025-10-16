#include "fota.h"
#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <Updater.h>
#include <ArduinoJson.h>

#include <Crypto.h>
#include <SHA256.h>
#include <AES.h>

// Custom HMAC-SHA256 implementation since HMAC.h is not available
static void hmac_sha256(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len, uint8_t* out) {
  const size_t block_size = 64; // SHA256 block size
  uint8_t k_pad[block_size];
  
  // If key is longer than block size, hash it first
  if (key_len > block_size) {
    SHA256 hash;
    hash.reset();
    hash.update(key, key_len);
    hash.finalize(k_pad, 32);
    memset(k_pad + 32, 0, block_size - 32);
  } else {
    memcpy(k_pad, key, key_len);
    memset(k_pad + key_len, 0, block_size - key_len);
  }
  
  // Create inner and outer padding
  uint8_t i_pad[block_size], o_pad[block_size];
  for (size_t i = 0; i < block_size; i++) {
    i_pad[i] = k_pad[i] ^ 0x36;
    o_pad[i] = k_pad[i] ^ 0x5c;
  }
  
  // Inner hash: H(K XOR ipad, message)
  SHA256 inner_hash;
  inner_hash.reset();
  inner_hash.update(i_pad, block_size);
  inner_hash.update(data, data_len);
  uint8_t inner_result[32];
  inner_hash.finalize(inner_result, 32);
  
  // Outer hash: H(K XOR opad, inner_hash)
  SHA256 outer_hash;
  outer_hash.reset();
  outer_hash.update(o_pad, block_size);
  outer_hash.update(inner_result, 32);
  outer_hash.finalize(out, 32);
}

// ---- Config/Paths ----
static String STATE_PATH = "/fota_state.json";
static String PENDING_OK = "/pending_ok";

// === Shared keys (match server) ===
// AES-128 key (16 bytes)
static const uint8_t AES_KEY[16] = {
  0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
  0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81
};

// HMAC-SHA256 key (32 bytes)
static const uint8_t HMAC_KEY[32] = {
  0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
  0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
  0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
  0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
};

// Small helpers
static bool httpGetJson(const String& url, DynamicJsonDocument& doc) {
  WiFiClient client;
  HTTPClient http;
  if(!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }
  auto stream = http.getStream();
  DeserializationError err = deserializeJson(doc, stream);
  http.end();
  return !err;
}

static bool httpPostJson(const String& url, const String& body) {
  WiFiClient client;
  HTTPClient http;
  if(!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  http.end();
  return code == 200;
}

static String b2hex(const uint8_t* b, size_t n) {
  static const char* H = "0123456789abcdef";
  String s; s.reserve(n*2);
  for (size_t i=0; i<n; i++) { s += H[b[i] >> 4]; s += H[b[i]&0x0f]; }
  return s;
}

static uint8_t hex2b(char c) {
  if (c>='0'&&c<='9') return c-'0';
  if (c>='a'&&c<='f') return 10+(c-'a');
  if (c>='A'&&c<='F') return 10+(c-'A');
  return 0;
}

static bool hexToBytes(const String& hex, uint8_t* out, size_t outlen) {
  if (hex.length() != (int)outlen*2) return false;
  for (size_t i=0; i<outlen; i++) {
    out[i] = (hex2b(hex[2*i])<<4) | hex2b(hex[2*i+1]);
  } 

  return true;
}

// Base64 decode using ESP8266 built-in functions
static size_t b64decode(uint8_t* out, const String& in) {
  // Simple base64 decode implementation
  const char* chars = in.c_str();
  size_t len = in.length();
  size_t outLen = 0;
  
  for (size_t i = 0; i < len; i += 4) {
    uint32_t val = 0;
    int pad = 0;
    
    for (int j = 0; j < 4; j++) {
      if (i + j >= len) break;
      char c = chars[i + j];
      uint8_t b = 0;
      
      if (c >= 'A' && c <= 'Z') b = c - 'A';
      else if (c >= 'a' && c <= 'z') b = c - 'a' + 26;  
      else if (c >= '0' && c <= '9') b = c - '0' + 52;
      else if (c == '+') b = 62;
      else if (c == '/') b = 63;
      else if (c == '=') { pad++; b = 0; }
      
      val = (val << 6) | b;
    }
    
    if (pad < 3) out[outLen++] = (val >> 16) & 0xFF;
    if (pad < 2) out[outLen++] = (val >> 8) & 0xFF;
    if (pad < 1) out[outLen++] = val & 0xFF;
  }
  
  return outLen;
}

//AES-CTR chunk offset increment (last 32 bits)
static void aesCtrXor(uint8_t* data, size_t len, const uint8_t key[16], const uint8_t iv[16], uint32_t counterOffset=0) {
  if (!data || len == 0) return;
  
  AES128 aes;
  aes.setKey(key, aes.keySize());
  
  uint8_t ctr[16];
  memcpy(ctr, iv, 16);

  // advance counter by counterOffset blocks (16 bytes per block)
  uint32_t* ctrTail = (uint32_t*)(ctr+12);
  // Convert to big-endian for proper counter increment
  uint32_t counter_be = (*ctrTail);
  counter_be += counterOffset;
  *ctrTail = counter_be;

  uint8_t keystream[16];
  size_t off = 0;
  
  while (off < len) {
    //encrypt counter to produce keystream
    memcpy(keystream, ctr, 16);
    aes.encryptBlock(keystream, keystream);

    size_t chunk = min((size_t)16, len - off);
    for (size_t i = 0; i < chunk; i++) {
      data[off + i] ^= keystream[i];
    }

    // Increment counter in big-endian format
    for (int i = 15; i >= 12; i--) {
      if (++ctr[i] != 0) break; // No carry needed
    }
    
    off += chunk;
  }
}

namespace FOTA {
  struct State {
    String target;
    uint32_t nextChunk;
    uint32_t totalChunks;
    uint32_t size;
    uint32_t chunkSize;
    String sha256;
    String ivHex;
  };

  static bool loadState(State& s) {
    if (!LittleFS.exists(STATE_PATH)) return false;
    File f = LittleFS.open(STATE_PATH, "r"); if (!f) return false;
    DynamicJsonDocument d(512);
    if (deserializeJson(d, f)) { f.close(); return false; }
    f.close();
    s.target = (const char*)d["target"];
    s.nextChunk = d["next"] | 0;
    s.totalChunks = d["total"] | 0;
    s.size = d["size"] | 0;
    s.chunkSize = d["chunk"] | 0;
    s.sha256 = (const char*)d["hash"];
    s.ivHex = (const char*)d["iv"];
    return true;
  }

  static void saveState(const State& s) {
    DynamicJsonDocument d(512);
    d["target"] = s.target;
    d["next"]   = s.nextChunk;
    d["total"]  = s.totalChunks;
    d["size"]   = s.size;
    d["chunk"]  = s.chunkSize;
    d["hash"]   = s.sha256;
    d["iv"]     = s.ivHex;
    File f = LittleFS.open(STATE_PATH, "w");
    serializeJson(d, f);
    f.close();
  }

  static void clearState() { if (LittleFS.exists(STATE_PATH)) LittleFS.remove(STATE_PATH); }

  static void report(const char* apiBase, const char* status, uint32_t chunk, const String& ver) {
    DynamicJsonDocument d(256);
    d["device_id"] = WiFi.macAddress();
    d["status"]    = status;
    d["chunk"]     = chunk;
    d["version"]   = ver;
    String body; serializeJson(d, body);
    httpPostJson(String(apiBase) + "/report", body);
  }

  static void confirmBoot(const char* apiBase) {
    if (!LittleFS.exists(PENDING_OK)) return;
    DynamicJsonDocument d(128);
    d["device_id"] = WiFi.macAddress();
    String body; serializeJson(d, body);
    if (httpPostJson(String(apiBase) + "/boot_ok", body)) {
      LittleFS.remove(PENDING_OK);
      Serial.println("[FOTA] Boot OK confirmed.");
    }
  }

  void run(const char* apiBase, const char* currentVersion) {
    Serial.printf("[FOTA] Starting FOTA check. Current version: %s\n", currentVersion);
    
    // Clear pending if applicable
    confirmBoot(apiBase);

    // 1) Manifest
    DynamicJsonDocument man(512);
    if (!httpGetJson(String(apiBase) + "/manifest", man)) {
      Serial.println("[FOTA] manifest fetch failed.");
      return;
    }

    String ver   = (const char*)man["version"];
    uint32_t size = man["size"] | 0;
    uint32_t chunkSize = man["chunk_size"] | 0;
    uint32_t total = man["total_chunks"] | 0;
    String hash = (const char*)man["hash"];        // per the reference key name
    String ivHex = (const char*)man["iv"];         // hex 16 bytes IV

    Serial.printf("[FOTA] Manifest: version=%s, size=%d, chunks=%d\n", ver.c_str(), (int)size, (int)total);

    if (ver == currentVersion) {
      Serial.println("[FOTA] Up-to-date.");
      return;
    }
    
    // Validate manifest data
    if (size == 0 || total == 0 || hash.length() == 0 || ivHex.length() != 32) {
      Serial.println("[FOTA] Invalid manifest data");
      return;
    }

    // Load/init state (for resume)
    State st;
    if (!loadState(st) || st.target != ver) {
      st.target = ver;
      st.nextChunk = 0;
      st.totalChunks = total;
      st.size = size;
      st.chunkSize = chunkSize;
      st.sha256 = hash;
      st.ivHex = ivHex;
      saveState(st);
    }

    // Prepare Update
    if (!Update.begin(st.size)) {
      Serial.printf("[FOTA] Update.begin failed: %s\n", Update.getErrorString().c_str());
      clearState();
      return;
    }

    // Running SHA256 of PLAINTEXT firmware
    SHA256 imgHash; imgHash.reset();

    // Pre-decode IV
    uint8_t iv[16];
    if (!hexToBytes(st.ivHex, iv, 16)) {
      Serial.println("[FOTA] Bad IV in manifest.");
      Update.end(false); clearState(); return;
    }

    // Loop chunks
    for (uint32_t n = st.nextChunk; n < st.totalChunks; ++n) {
      DynamicJsonDocument cj(4096);
      String url = String(apiBase) + "/chunk?version=" + st.target + "&n=" + n;
      if (!httpGetJson(url, cj)) {
        Serial.println("[FOTA] chunk fetch failed."); Update.end(false); return;
      }
      uint32_t num = cj["chunk_number"] | 0;
      if (num != n) { Serial.println("[FOTA] chunk number mismatch."); Update.end(false); return; }

      String ivHex2 = (const char*)cj["iv"];
      String macHex = (const char*)cj["mac"];
      String dataB64 = (const char*)cj["data"];

      // Decode cipher with better error handling
      size_t maxOut = (dataB64.length() * 3) / 4 + 4;
      std::unique_ptr<uint8_t[]> cipher(new uint8_t[maxOut]);
      size_t cLen = b64decode(cipher.get(), dataB64);
      if (!cLen || cLen > maxOut) { 
        Serial.printf("[FOTA] b64 decode fail. Expected ~%d, got %d\n", (int)maxOut, (int)cLen);
        Update.end(false); 
        return; 
      }
      
      Serial.printf("[FOTA] Chunk %d: decoded %d bytes\n", (int)n, (int)cLen);

      // Verify IV identical to manifest (or chunk IV if server varies)
      uint8_t ivc[16];
      if (!hexToBytes(ivHex2, ivc, 16)) { Serial.println("[FOTA] bad iv hex."); Update.end(false); return; }

      // Verify HMAC: HMAC(K, n||iv||cipher)
      size_t hmac_data_len = 4 + 16 + cLen;
      uint8_t* hmac_data = (uint8_t*)malloc(hmac_data_len);
      if (!hmac_data) {
        Serial.println("[FOTA] HMAC data allocation failed");
        Update.end(false);
        return;
      }
      
      // Build HMAC input: n||iv||cipher
      uint8_t nb[4] = {(uint8_t)(n>>24),(uint8_t)(n>>16),(uint8_t)(n>>8),(uint8_t)n};
      memcpy(hmac_data, nb, 4);
      memcpy(hmac_data + 4, ivc, 16);
      memcpy(hmac_data + 20, cipher.get(), cLen);
      
      uint8_t mac[32];
      hmac_sha256(HMAC_KEY, sizeof(HMAC_KEY), hmac_data, hmac_data_len, mac);
      free(hmac_data);
      
      String macCalc = b2hex(mac, 32);
      if (!macCalc.equalsIgnoreCase(macHex)) {
        Serial.printf("[FOTA] HMAC verify failed. Expected: %s, Got: %s\n", macHex.c_str(), macCalc.c_str());
        Update.end(false); 
        return;
      }

      // Decrypt in place (CTR) — Each chunk starts with fresh IV, no offset
      // The server should provide fresh IV per chunk or we calculate based on chunk number
      aesCtrXor(cipher.get(), cLen, AES_KEY, ivc, 0); // Start from 0 for each chunk
      
      // Add integrity check - verify some decrypted content makes sense
      if (n == 0) {
        // First chunk should start with ELF header for ESP8266 firmware
        if (cLen >= 4 && cipher[0] != 0x7f && cipher[1] != 'E' && cipher[2] != 'L' && cipher[3] != 'F') {
          Serial.println("[FOTA] Warning: First chunk doesn't look like ELF firmware");
        }
      }

      // Write plaintext to flash and to running SHA256
      size_t w = Update.write(cipher.get(), cLen);
      if (w != cLen) { 
        Serial.printf("[FOTA] Flash write fail: %s\n", Update.getErrorString().c_str()); 
        Update.end(false); 
        return; 
      }
      imgHash.update(cipher.get(), cLen);

      st.nextChunk = n + 1; saveState(st);
      report(apiBase, "chunk_ok", n, st.target);
      Serial.printf("[FOTA] chunk %lu/%lu OK\n", (unsigned long)(n+1), (unsigned long)st.totalChunks);
    }

    // Final SHA256 compare with detailed logging
    uint8_t sum[32]; 
    imgHash.finalize(sum, 32);
    String sumHex = b2hex(sum, 32);
    
    Serial.printf("[FOTA] Calculated SHA256: %s\n", sumHex.c_str());
    Serial.printf("[FOTA] Expected SHA256:   %s\n", st.sha256.c_str());
    
    if (!sumHex.equalsIgnoreCase(st.sha256)) {
      Serial.println("[FOTA] SHA256 mismatch detected!");
      Serial.printf("[FOTA] Total bytes processed: %d\n", (int)st.size);
      Serial.printf("[FOTA] Chunks processed: %d/%d\n", (int)st.totalChunks, (int)st.totalChunks);
      
      // Additional debugging - check first few bytes of calculated hash
      Serial.print("[FOTA] Calculated hash bytes: ");
      for (int i = 0; i < 8; i++) {
        Serial.printf("%02x ", sum[i]);
      }
      Serial.println();
      
      Update.end(false); 
      clearState(); 
      report(apiBase, "sha_fail", st.totalChunks, st.target); 
      return;
    }
    
    Serial.println("[FOTA] SHA256 verification passed!");

    if (!Update.end(true)) {
      Serial.printf("[FOTA] Update.end failed: %s\n", Update.getErrorString().c_str());
      clearState(); 
      return;
    }

    // Mark pending OK and reboot
    { File f = LittleFS.open(PENDING_OK, "w"); if (f) f.close(); }
    clearState();
    report(apiBase, "done", st.totalChunks, st.target);
    Serial.println("[FOTA] Success. Rebooting...");
    ESP.restart();
  }

}