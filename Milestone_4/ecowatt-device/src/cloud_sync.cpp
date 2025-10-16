#include "cloud_sync.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "command_exe.h"

extern const char* API_BASE;  // defined in main.cpp

// helper for GET requests
static bool http_get_json(const String& url, String& out) {
  WiFiClient client; HTTPClient http;
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != 200) { http.end(); return false; }
  out = http.getString();
  http.end();
  return true;
}

// helper for POST
static bool http_post_json(const String& url, const String& body) {
  WiFiClient client; HTTPClient http;
  if (!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  http.end();
  return (code >= 200 && code < 300);
}

void cloud_sync_cycle() {
  String device_id = WiFi.macAddress();

  // ======== 1. Fetch config update ========
  String cfg_json;
  String cfg_url = String(API_BASE) + "/device/" + device_id + "/config";
  if (http_get_json(cfg_url, cfg_json)) {
    Serial.println("[CLOUD] Config JSON received:");
    Serial.println(cfg_json);
    String accepted_json, rejected_json;
    bool ok = config_stage_from_json(cfg_json, accepted_json, rejected_json);
    String ack = config_build_ack_json();
    String ack_url = String(API_BASE) + "/device/" + device_id + "/config_ack";
    http_post_json(ack_url, ack);
    if (ok) {
      Serial.println("[CLOUD] Staged config, will apply next cycle");
    } else {
      Serial.println("[CLOUD] Config rejected");
      Serial.println(rejected_json);
    }
  }

  // ======== 2. Fetch commands ========
  String cmd_url = String(API_BASE) + "/device/" + device_id + "/commands";
  String cmd_json;
  if (http_get_json(cmd_url, cmd_json)) {
    Serial.println("[CLOUD] Command JSON received:");
    Serial.println(cmd_json);
    CommandResult r;
    if (command_process_response_json(cmd_json, r)) {
      String res = command_result_to_json(r);
      String res_url = String(API_BASE) + "/device/" + device_id + "/command_result";
      http_post_json(res_url, res);
      Serial.println("[CLOUD] Command executed and reported");
    } else {
      Serial.println("[CLOUD] No valid command in JSON");
    }
  }

  // ======== 3. Apply staged config ========
  config_apply_staged();  // activates staged config now that upload finished
}
