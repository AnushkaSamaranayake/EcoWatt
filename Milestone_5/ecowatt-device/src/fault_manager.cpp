#include "fault_manager.h"
#include <LittleFS.h>
#include <Arduino.h>

static FaultEvent events[32];
static size_t ev_head = 0;
static size_t ev_count = 0;

void fault_init(){
  if (!LittleFS.begin()) {
    // If LittleFS already started elsewhere this may fail; ignore
  }
}

static void print_fault_banner() {
  Serial.println();
  Serial.println(F("==================================="));
  Serial.println(F("FAULT EVENT DETECTED"));
  Serial.println(F("==================================="));
}

static void print_recovery_banner() {
  Serial.println();
  Serial.println(F("==================================="));
  Serial.println(F("RECOVERY EVENT"));
  Serial.println(F("==================================="));
}


void fault_log(const char* code, const char* details){
  // Use new typed version with automatic classification
  ErrorType type = classify_error(code);
  fault_log_typed(code, details, type);
}

void fault_log_typed(const char* code, const char* details, ErrorType type){
  uint32_t now = millis();
  FaultEvent e;
  e.ts_ms = now;
  e.error_type = type;
  strncpy(e.code, code, sizeof(e.code)-1); e.code[sizeof(e.code)-1]=0;
  strncpy(e.details, details, sizeof(e.details)-1); e.details[sizeof(e.details)-1]=0;

  // ---- SERIAL MONITOR OUTPUT ----
  print_fault_banner();
  Serial.printf("Time (ms)  : %lu\n", (unsigned long)e.ts_ms);
  Serial.printf("Error Code : %s\n", e.code);
  Serial.printf("Type       : %s\n", error_type_to_string(type));
  Serial.printf("Details    : %s\n", e.details);
  Serial.println(F("==================================="));

  // store in circular buffer
  size_t idx = (ev_head + ev_count) % (sizeof(events)/sizeof(events[0]));
  events[idx] = e;
  if (ev_count < (sizeof(events)/sizeof(events[0]))) ev_count++; 
  else ev_head = (ev_head+1) % (sizeof(events)/sizeof(events[0]));

  // also append to file for persistence with type
  File f = LittleFS.open("/fault_log.txt", "a");
  if (f) {
    f.printf("%lu,%s,%s,%s\n", (unsigned long)e.ts_ms, e.code, 
             error_type_to_string(type), e.details);
    f.close();
  }
}

size_t fault_get_recent(FaultEvent* out, size_t max_items){
  size_t c = (ev_count < max_items) ? ev_count : max_items;
  for (size_t i = 0; i < c; i++){
    size_t idx = (ev_head + ev_count - c + i) % (sizeof(events)/sizeof(events[0]));
    out[i] = events[idx];
  }
  return c;
}

void fault_persist(){
  // Already appended during logging; this is a noop but kept for API completeness
}

void fault_log_recovery(const char* code, const char* module) {
    FaultEvent e;
    e.ts_ms = millis();
    e.error_type = ErrorType::NONE;   // Recovery state
    strncpy(e.code, code, sizeof(e.code)-1);
    snprintf(e.details, sizeof(e.details), "recovered in %s", module);

    print_recovery_banner();

    Serial.printf("Time (ms)  : %lu\n", (unsigned long)e.ts_ms);
    Serial.printf("Recovered  : %s\n", e.code);
    Serial.printf("Module     : %s\n", module);

    Serial.println(F("==================================="));


    size_t idx = (ev_head + ev_count) % (sizeof(events)/sizeof(events[0]));
    events[idx] = e;
    if (ev_count < (sizeof(events)/sizeof(events[0]))) ev_count++;
    else ev_head = (ev_head + 1) % (sizeof(events)/sizeof(events[0]));

    File f = LittleFS.open("/fault_log.txt", "a");
    if (f) {
        f.printf("%lu,%s,RECOVERY,%s\n",
                (unsigned long)e.ts_ms, code, module);
        f.close();
    }
}


// ============ Error Classification Helpers ============

ErrorType classify_error(const char* code) {
  if (strstr(code, "http")) return ErrorType::HTTP_FAIL;
  if (strstr(code, "modbus_exception")) return ErrorType::MODBUS_EXCEPTION;
  if (strstr(code, "crc")) return ErrorType::CRC_ERROR;
  if (strstr(code, "corrupt")) return ErrorType::CORRUPT_FRAME;
  if (strstr(code, "timeout")) return ErrorType::TIMEOUT;
  if (strstr(code, "packet_drop") || strstr(code, "empty_frame")) return ErrorType::PACKET_DROP;
  return ErrorType::UNKNOWN;
}

bool is_retryable_error(ErrorType type) {
  switch (type) {
    case ErrorType::HTTP_FAIL:
    case ErrorType::TIMEOUT:
    case ErrorType::PACKET_DROP:
      return true;  // These errors should be retried
    
    case ErrorType::MODBUS_EXCEPTION:
    case ErrorType::CRC_ERROR:
    case ErrorType::CORRUPT_FRAME:
      return false;  // These indicate data integrity issues, don't retry
    
    default:
      return false;
  }
}

const char* error_type_to_string(ErrorType type) {
  switch (type) {
    case ErrorType::NONE: return "NONE";
    case ErrorType::HTTP_FAIL: return "HTTP_FAIL";
    case ErrorType::MODBUS_EXCEPTION: return "MODBUS_EXCEPTION";
    case ErrorType::CRC_ERROR: return "CRC_ERROR";
    case ErrorType::CORRUPT_FRAME: return "CORRUPT_FRAME";
    case ErrorType::TIMEOUT: return "TIMEOUT";
    case ErrorType::PACKET_DROP: return "PACKET_DROP";
    case ErrorType::UNKNOWN: return "UNKNOWN";
    default: return "INVALID";
  }
}
