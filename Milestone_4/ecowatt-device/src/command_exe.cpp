#include "command_exe.h"
#include "protocol_adapter.h"
#include <ArduinoJson.h>

void command_init() {
  // Nothing yet
}

uint16_t map_register_name_to_addr(const String& reg) {
  if (reg == "export_power") return 8;
  if (reg == "voltage") return 0;
  if (reg == "current") return 1;
  if (reg == "frequency") return 2;
  return 0xFFFF;
}

bool command_process_response_json(const String& respBody, CommandResult& out) {
  StaticJsonDocument<512> doc;
  auto err = deserializeJson(doc, respBody);
  if (err) return false;
  if (!doc.containsKey("command")) return false;
  JsonObject c = doc["command"];
  out.id = c.containsKey("id") ? c["id"].as<String>() : "cmd";
  String action = c["action"].as<String>();
  if (action == "write_register") {
    String target = c["target_register"].as<String>();
    int value = c["value"].as<int>();
    uint16_t addr = map_register_name_to_addr(target);
    if (addr == 0xFFFF) {
      out.success = false; out.details = "unknown register"; return true;
    }
    bool ok = writeSingleRegister(addr, (uint16_t)value);
    out.success = ok; out.details = ok ? "ok" : "no_response";
    out.executed_at = String(millis());
    return true;
  }
  return false;
}

String command_result_to_json(const CommandResult& r) {
  DynamicJsonDocument d(256);
  JsonObject o = d.createNestedObject("command_result");
  o["id"] = r.id;
  o["status"] = r.success ? "success" : "failed";
  o["details"] = r.details;
  o["executed_at"] = r.executed_at;
  String s; serializeJson(d, s);
  return s;
}