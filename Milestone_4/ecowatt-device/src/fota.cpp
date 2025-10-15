#include "fota.h"
#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <Updater.h>
#include <ArduinoJson.h>

#include <Crypto.h>
#include <SHA256.h>
#include <HMAC.h>
#include <AES.h>

// ---- Config/Paths ----
static String STATE_PATH = "/fota_state.json";
static String PENDIGN_OK = "/pending_ok";

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

static bool httpPostJson(cosnt String& url, const String& body) {
  WiFiClient client;
  HTTPClient http;
  if(!http.begin(client, url)) return false;
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

//Base64 decode (ESP8266 core)
extern "C" {
  #include "libb64/cdecode.h
}

static size_t b64decode(uint8_t* out, const String& in) {
  base64_decodestate st; base64_init_decodestate(&st);
  int outlen = base64_decode_block(in.c_str(), in.length(), (char*)out, &st);
  return outlen < 0 ? 0 : (size_t)outlen; 
}

//AES-CTR chunk offset increment (last 32 bits)
static void aesCtrXor(uint8_t* data, size_t len, const uint8_t key[16], const uint8_t iv[16],  uint32_t counterOffset=0) {
  AES128 aes;
  uint8_t ctr[16];
  memcpy(ctr, iv, 16);

  // advance counter by counterOffset blocks (16 bytes per block)
  uint32_t* ctrTail = (uint32_t*)(ctr+12);
  *ctrTail = *ctrTail + counterOffset;

  uint8_t keystream[16];
  size_t off = 0;
  while (off < len) {
    //encrypt counter to produce keystream
    memcpy(keystream, ctr, 16);
    aes.setKey(key, aes.keySize());
    aes.encryptBlock(keystream, keystream);

    size_t chunk = min((size_t)16, len - off);
    for (size_t i=0; i<chunk; i++) data[off+i] ^= keystream[i];

    (*ctrTail)++;
    off += chunk;
  }
}

namespace FOTA {
  struct State {
    String target;
    uint32_t nextChunk;
    uint32_t totalChunks;
    uint32_t size;
    uint32_t ChunkSize;
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

    if (ver == currentVersion) {
      Serial.println("[FOTA] Up-to-date.");
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
      Serial.printf("[FOTA] Update.begin failed: %s\n", Update.errorString());
      clearState();
      return;
    }

    // Running SHA256 of PLAINTEXT firmware
    SHA256 imgHash; imgHash.reset();

    // Pre-decode IV
    uint8_t iv[16];
    if (!hexToBytes(st.ivHex, iv, 16)) {
      Serial.println("[FOTA] Bad IV in manifest.");
      Update.abort(); clearState(); return;
    }

    // Loop chunks
    for (uint32_t n = st.nextChunk; n < st.totalChunks; ++n) {
      DynamicJsonDocument cj(4096);
      String url = String(apiBase) + "/chunk?version=" + st.target + "&n=" + n;
      if (!httpGetJson(url, cj)) {
        Serial.println("[FOTA] chunk fetch failed."); Update.abort(); return;
      }
      uint32_t num = cj["chunk_number"] | 0;
      if (num != n) { Serial.println("[FOTA] chunk number mismatch."); Update.abort(); return; }

      String ivHex2 = (const char*)cj["iv"];
      String macHex = (const char*)cj["mac"];
      String dataB64 = (const char*)cj["data"];

      // Decode cipher
      size_t maxOut = (dataB64.length() * 3) / 4 + 4;
      std::unique_ptr<uint8_t[]> cipher(new uint8_t[maxOut]);
      size_t cLen = b64decode(cipher.get(), dataB64);
      if (!cLen) { Serial.println("[FOTA] b64 decode fail."); Update.abort(); return; }

      // Verify IV identical to manifest (or chunk IV if server varies)
      uint8_t ivc[16];
      if (!hexToBytes(ivHex2, ivc, 16)) { Serial.println("[FOTA] bad iv hex."); Update.abort(); return; }

      // Verify HMAC: HMAC(K, n||iv||cipher)
      HMAC<SHA256> hmac;
      hmac.reset(HMAC_KEY, sizeof(HMAC_KEY));
      uint8_t nb[4] = {(uint8_t)(n>>24),(uint8_t)(n>>16),(uint8_t)(n>>8),(uint8_t)n};
      hmac.update(nb, 4);
      hmac.update(ivc, 16);
      hmac.update(cipher.get(), cLen);
      uint8_t mac[32]; hmac.finalize(mac, sizeof(mac));
      String macCalc = b2hex(mac, 32);
      if (!macCalc.equalsIgnoreCase(macHex)) {
        Serial.println("[FOTA] HMAC verify failed."); Update.abort(); return;
      }

      // Decrypt in place (CTR) — counterOffset = (chunk_bytes / 16) * n? We reset counter per chunk using chunk offset blocks.
      // We'll compute offset (blocks) = (n * chunkSize)/16
      uint32_t blockOffset = ( (uint64_t)n * (uint64_t)st.chunkSize ) / 16;
      aesCtrXor(cipher.get(), cLen, ENC_KEY, ivc, blockOffset);

      // Write plaintext to flash and to running SHA256
      size_t w = Update.write(cipher.get(), cLen);
      if (w != cLen) { Serial.printf("[FOTA] Flash write fail: %s\n", Update.errorString()); Update.abort(); return; }
      imgHash.update(cipher.get(), cLen);

      st.nextChunk = n + 1; saveState(st);
      report(apiBase, "chunk_ok", n, st.target);
      Serial.printf("[FOTA] chunk %lu/%lu OK\n", (unsigned long)(n+1), (unsigned long)st.totalChunks);
    }

    // Final SHA256 compare
    uint8_t sum[32]; imgHash.finalize(sum, 32);
    String sumHex = b2hex(sum, 32);
    if (!sumHex.equalsIgnoreCase(st.sha256)) {
      Serial.println("[FOTA] Final SHA256 mismatch → rollback.");
      Update.abort(); clearState(); report(apiBase, "sha_fail", st.totalChunks, st.target); return;
    }

    if (!Update.end(true)) {
      Serial.printf("[FOTA] Update.end failed: %s\n", Update.errorString());
      clearState(); return;
    }

    // Mark pending OK and reboot
    { File f = LittleFS.open(PENDING_OK, "w"); if (f) f.close(); }
    clearState();
    report(apiBase, "done", st.totalChunks, st.target);
    Serial.println("[FOTA] Success. Rebooting...");
    ESP.restart();
  }

}