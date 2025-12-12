#include "config.h"
#include <EEPROM.h>
#include <Arduino.h>  
#include <ArduinoJson.h>

#define EEPROM_SIZE 2048
#define CFG_ADDR 0

static AppConfig active_cfg;
static AppConfig staged_cfg;
static bool has_staged = false;

static AppConfig default_cfg(){
  AppConfig c;
  c.version = 1;
  c.sampling_interval_s = 5;
  c.upload_interval_s = 15;
  c.register_mask = 0x03;
  c.max_buffer_size = 256;
  return c;
}

void config_init(){
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(CFG_ADDR, active_cfg);
  if (active_cfg.sampling_interval_s == 0 || active_cfg.max_buffer_size == 0) {
    active_cfg = default_cfg();
    EEPROM.put(CFG_ADDR, active_cfg);
    EEPROM.commit();
  }
  // active_cfg = default_cfg();
  // EEPROM.put(CFG_ADDR, active_cfg);
  // EEPROM.commit();
  // Serial.println("[CFG] EEPROM RESET TO DEFAULT");

  has_staged = false;
}

static bool validate(const AppConfig& c){
  if (c.sampling_interval_s < 1 || c.sampling_interval_s > 3600) return false;
  if (c.upload_interval_s < 15 || c.upload_interval_s > 3600) return false;
  if (c.max_buffer_size == 0 || c.max_buffer_size > 4096) return false;
  // register_mask allowed bits 0..7 in current design
  return true;
}

AppConfig config_get_active(){ 
  return active_cfg; 
}

bool config_stage_from_json(const String& json, String& accepted_json, String& rejected_json){
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, json)) {
    rejected_json = "{\"rejected\":[\"invalid_json\"]}";
    return false;
  }
  if (!doc.containsKey("config_update")) {
    rejected_json = "{\"rejected\":[\"no_config_update\"]}";
    return false;
  }

  JsonObject u = doc["config_update"];
  staged_cfg = active_cfg;
  bool changed = false;

  if (u.containsKey("sampling_interval")) {
    staged_cfg.sampling_interval_s = u["sampling_interval"].as<uint32_t>();
    changed = true;
  }
  if (u.containsKey("upload_interval")) {
    staged_cfg.upload_interval_s = u["upload_interval"].as<uint32_t>();
    changed = true;
  }
  if (u.containsKey("max_buffer_size")) {
    staged_cfg.max_buffer_size = u["max_buffer_size"].as<uint16_t>();
    changed = true;
  }
  if (u.containsKey("registers")) {
    uint8_t mask = 0;
    for (JsonVariant v : u["registers"].as<JsonArray>()) {
      String r = v.as<String>();
      if (r == "voltage")   mask |= (1 << 0);
      if (r == "current")   mask |= (1 << 1);
      if (r == "frequency") mask |= (1 << 2);
    }
    staged_cfg.register_mask = mask;
    changed = true;
  }

  if (!changed) {
    rejected_json = "{\"rejected\":[\"no_changes\"]}";
    return false;
  }
  if (!validate(staged_cfg)) {
    rejected_json = "{\"rejected\":[\"invalid_values\"]}";
    return false;
  }

  staged_cfg.version = active_cfg.version + 1;
  has_staged = true;
  accepted_json = "{\"accepted\":[\"staged\"]}";
  return true;
}

void config_apply_staged(){
  if (!has_staged) return;
  noInterrupts();
  active_cfg = staged_cfg;
  has_staged = false;
  EEPROM.put(CFG_ADDR, active_cfg);
  EEPROM.commit();
  interrupts();
}

String config_build_ack_json(){
  String s =
    "{\"config_ack\":{"
      "\"accepted\":[\"sampling_interval\",\"upload_interval\",\"registers\"],"
      "\"rejected\":[],"
      "\"unchanged\":[]"
    "}}";
  return s;
}
