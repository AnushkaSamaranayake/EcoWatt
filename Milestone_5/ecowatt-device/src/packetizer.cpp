#include "packetizer.h"
#include "buffer.h"
#include "compressor.h"
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "security.h"
#include <ArduinoJson.h>
#include "cloud_sync.h"
#include "config_filter.h"

#include <base64.h> // if using base64 lib; otherwise implement simple base64

extern const char* URL_UPLOAD; // define in main
extern const char* API_KEY_1;

bool finalize_and_upload(const char* device_id, unsigned long interval_start_ms, uint8_t* workbuf, size_t workcap){
  size_t cnt = buffer_count();
  if (cnt == 0) return false;
  
  // Allocate memory with safety check
  Sample* samples = (Sample*)malloc(sizeof(Sample) * cnt);
  if (!samples) {
    Serial.println("[ERROR] Failed to allocate memory for samples");
    buffer_clear(); // Clear buffer to prevent continuous failures
    return false;
  }
  
  size_t out_n = 0;
  buffer_drain_all(samples, out_n);
  if (out_n == 0) { 
    free(samples); 
    return false; 
  }

  // compress into workbuf
  // size_t compressed = compress_delta_rle(samples, out_n, workbuf, workcap);
  uint8_t mask = config_get_active().register_mask;
  size_t compressed = compress_delta_rle_v2(
    samples, out_n, mask, workbuf, workcap
  );

  if (compressed == 0) {
    free(samples);
    // <<< prevent slowly accumulating partial states
    buffer_clear();    // <<<
    return false;
  }

  // produce hex payload with size limit check
  String base64payload = "";
  base64payload.reserve(compressed * 2 + 1); // Pre-allocate memory
  
  for (size_t i = 0; i < compressed; i++){
    char tmp[3];
    sprintf(tmp, "%02X", workbuf[i]);
    base64payload += tmp;
    
    // Safety check to prevent memory exhaustion
    if (base64payload.length() > 4096) {
      Serial.println("[ERROR] Payload too large, truncating");
      break;
    }
  }

  // compute aggregates (currently unused but fine)
  int16_t minV=INT16_MAX, maxV=INT16_MIN, sumV=0;
  for (size_t i=0;i<out_n;i++){ int16_t v=samples[i].voltage; if (v<minV) minV=v; if (v>maxV) maxV=v; sumV+=v; }
  float avgV = sumV / (float)out_n; (void)avgV; // optional: silence unused warning

  // uint8_t mask = config_get_active().register_mask;

  // Build JSON with size control
  String body = "";
  body.reserve(2048); // Pre-allocate reasonable amount
  
  body += "{";
  body += "\"device_id\":\"" + String(device_id) + "\"";
  body += ",\"interval_start\":" + String(interval_start_ms);
  body += ",\"sample_count\":" + String(out_n);
  // body += ",\"payload_format\":\"DELTA_RLE_v1\"";
  body += ",\"payload_format\":\"DELTA_RLE_v2\"";
  body += ",\"channel_mask\":" + String(mask);
  body += ",\"payload\":\"" + base64payload + "\"";

  // Limit the number of individual voltage/current samples to prevent overflow
  size_t max_samples = min(out_n, (size_t)50); // Limit to 50 samples max
  
  body += ",\"voltage\":[";
  if(should_read_voltage()) {
    for (size_t i = 0; i < max_samples; i++){
      body += String(samples[i].voltage / 1.0, 2); // 2 decimal places
      if (i < max_samples - 1) body += ",";
    }
  }
  body += "]";

  body += ",\"current\":[";
  if(should_read_current()) {
    for (size_t i = 0; i < max_samples; i++){
        body += String(samples[i].current / 10.0, 2); // 2 decimal places  
        if (i < max_samples - 1) body += ",";
    }
  }

  body += "]";

  body += ",\"frequency\":[";
  if (should_read_frequency()) {
    for (size_t i = 0; i < max_samples; i++) {
      body += String(samples[i].frequency, 2);
      if (i < max_samples - 1) body += ",";
    }
  }
  body += "]";

  body += ",\"temperature\":[";
  if (should_read_temperature()) {
    for (size_t i = 0; i < max_samples; i++) {
      body += String(samples[i].temperature, 2);
      if (i < max_samples - 1) body += ",";
    }
  }
  body += "]";

  body += "}";
  
  // Safety check for body size
  if (body.length() > 3072) { // 3KB limit
    Serial.println("[ERROR] Body too large, upload cancelled");
    free(samples);
    return false;
  }

  Serial.println("[UPLOAD] Posting JSON:");
  Serial.println(body);

  // secure envelope (HMAC over EXACT body string)
  uint32_t nonce = nonce_load() + 1;
  String mac = compute_hmac_hex((const uint8_t*)API_KEY_1, strlen(API_KEY_1),
                               (const uint8_t*)body.c_str(), body.length());
  String envelope = build_envelope_json(nonce, body, mac);
  Serial.println("[UPLOAD] Posting ENVELOPE:");
  Serial.println(envelope);

  // send HTTP POST
  WiFiClient client;
  HTTPClient http;

  // <<< extra debug
  Serial.print("[UPLOAD] URL: "); Serial.println(URL_UPLOAD); // <<<
  if (!http.begin(client, URL_UPLOAD)) {
    Serial.println("[UPLOAD] http.begin() failed");
    free(samples);
    buffer_clear();  // <<< ensure no overflow if we can't even start HTTP
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-device-id", device_id);
  http.addHeader("x-api-key", API_KEY_1);

  int code = http.POST(envelope);
  String resp = http.getString();
  http.end();
  free(samples);

  if (code != 200) {
    Serial.printf("[UPLOAD] HTTP %d\n", code);
    Serial.println(resp);
    buffer_clear();   // <<< clear on any failed POST to avoid overflow
    return false;
  }
  Serial.println("[UPLOAD] success: " + resp);

  // ==== cloud Sync (pull configs/commands)
  cloud_sync_cycle();

  // parse response envelope and update nonce
  SecuredEnvelope env;
  if (parse_envelope_json(resp, env)) {
    nonce_save(env.nonce);
  }
  return true;
}
