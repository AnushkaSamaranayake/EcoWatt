#include "command_exe.h"
#include "protocol_adapter.h"
#include "fault_manager.h"
#include <ArduinoJson.h>

// Global flag to simulate error injection
static struct {
  bool active;
  String errorType;
  int exceptionCode;
  int delayMs;
} g_error_injection = {false, "", 0, 0};

void command_init() {
  // Nothing yet
}

// Check if error injection is active and apply it
bool should_inject_error() {
  return g_error_injection.active;
}

void apply_error_injection() {
  if (!g_error_injection.active) return;
  
  String errorType = g_error_injection.errorType;
  Serial.println("[ERROR_INJECT] Simulating error: " + errorType);
  
  if (errorType == "EXCEPTION") {
    int code = g_error_injection.exceptionCode;
    fault_log_typed("injected_exception", 
                   (String("Simulated exception code ") + String(code)).c_str(),
                   ErrorType::MODBUS_EXCEPTION);
    Serial.printf("[ERROR_INJECT] Modbus Exception %d\n", code);
    
  } else if (errorType == "CRC_ERROR") {
    fault_log_typed("injected_crc_error", 
                   "Simulated CRC error",
                   ErrorType::CRC_ERROR);
    Serial.println("[ERROR_INJECT] CRC Error");
    
  } else if (errorType == "CORRUPT") {
    fault_log_typed("injected_corrupt_frame", 
                   "Simulated corrupt frame",
                   ErrorType::CORRUPT_FRAME);
    Serial.println("[ERROR_INJECT] Corrupt Frame");
    
  } else if (errorType == "PACKET_DROP") {
    fault_log_typed("injected_packet_drop", 
                   "Simulated packet drop",
                   ErrorType::PACKET_DROP);
    Serial.println("[ERROR_INJECT] Packet Drop");
    
  } else if (errorType == "DELAY") {
    int delayMs = g_error_injection.delayMs;
    fault_log_typed("injected_delay", 
                   (String("Simulated delay ") + String(delayMs) + "ms").c_str(),
                   ErrorType::TIMEOUT);
    Serial.printf("[ERROR_INJECT] Delay %dms\n", delayMs);
    delay(delayMs);
  }
  
  // Clear the injection flag after use (one-time)
  g_error_injection.active = false;
}

uint16_t map_register_name_to_addr(const String& reg) {
  if (reg == "export_power") return 8;
  if (reg == "status_flag") return 8;
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
  
  // Handle error injection command
  if (action == "inject_error") {
    String errorType = c["error_type"].as<String>();
    g_error_injection.active = true;
    g_error_injection.errorType = errorType;
    g_error_injection.exceptionCode = c.containsKey("exception_code") ? c["exception_code"].as<int>() : 0;
    g_error_injection.delayMs = c.containsKey("delay_ms") ? c["delay_ms"].as<int>() : 0;
    
    Serial.println("[COMMAND] Error injection armed: " + errorType);
    
    // Apply the error immediately
    apply_error_injection();
    
    out.success = true;
    out.details = "Error injected: " + errorType;
    out.executed_at = String(millis());
    return true;
  }
  
  // Handle normal write_register command
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