#include "packetizer.h"
#include "buffer.h"
#include "compressor.h"
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "security.h"
#include <ArduinoJson.h>
#include "cloud_sync.h"

#include <base64.h> // if using base64 lib; otherwise implement simple base64

extern const char* URL_UPLOAD; // define in main
extern const char* DEVICE_PSK;

bool finalize_and_upload(const char* device_id, unsigned long interval_start_ms, uint8_t* workbuf, size_t workcap){
  size_t cnt = buffer_count();
  if (cnt==0) return false;
  Sample* samples = (Sample*)malloc(sizeof(Sample)*cnt);
  size_t out_n=0;
  buffer_drain_all(samples, out_n);
  if (out_n==0){ free(samples); return false; }

  // compress into workbuf
  size_t compressed = compress_delta_rle(samples, out_n, workbuf, workcap);
  if (compressed == 0) {
    free(samples);
    return false;
  }

  // produce base64 payload
  // simple Base64: use Arduino base64 library if available; else send hex for demo.
  String base64payload = ""; // fallback to hex if base64 lib not available
  for (size_t i=0;i<compressed;i++){
    char tmp[3];
    sprintf(tmp, "%02X", workbuf[i]);
    base64payload += tmp;
  }

  // compute aggregates
  int16_t minV=INT16_MAX, maxV=INT16_MIN, sumV=0;
  for (size_t i=0;i<out_n;i++){ int16_t v=samples[i].voltage; if (v<minV) minV=v; if (v>maxV) maxV=v; sumV+=v; }
  float avgV = sumV / (float)out_n;

  // build JSON
  String body = "{";
  body += "\"device_id\":\""; body += device_id; body += "\"";
  body += ",\"interval_start\":"; body += String(interval_start_ms);
  body += ",\"sample_count\":"; body += String(out_n);
  body += ",\"payload_format\":\"DELTA_RLE_v1\"";
  body += ",\"payload\":\""; body += base64payload; body += "\"";


  // add raw voltage
  body += ",\"voltage\":[";
  for (size_t i=0; i<out_n; i++){
    body += String(samples[i].voltage/10.0);
    if (i <out_n-1) body += ",";
  }
  body += "]";

  // add raw current array
  body += ",\"current\":[";
  for (size_t i=0; i<out_n; i++){
    body += String(samples[i].current/10.0);
    if (i <out_n-1) body += ",";
  }
  body += "]";

  body += "}";

    // --- Debug print before sending ---
  Serial.println("[UPLOAD] Posting JSON:");
  Serial.println(body);

  // secure envelope
  uint32_t nonce = nonce_load() + 1;
  String mac = compute_hmac_hex((const uint8_t*)DEVICE_PSK, strlen(DEVICE_PSK),
                               (const uint8_t*)body.c_str(), body.length());
  String envelope = build_envelope_json(nonce, body, mac);

  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, URL_UPLOAD)) { free(samples); return false; }
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(envelope);
  String resp = http.getString();
  http.end();
  free(samples);

  if (code != 200) {
    Serial.printf("[UPLOAD] HTTP %d\n", code);
    Serial.println(resp);
    return false;
  }
  Serial.println("[UPLOAD] success: " + resp);

  // ==== cloud Sync
  cloud_sync_cycle();

  // parse response envelope and update nonce
  SecuredEnvelope env;
  if (parse_envelope_json(resp, env)) {
    // verify server mac if required (left for server implementation)
    nonce_save(env.nonce);
  }
  return true;
}