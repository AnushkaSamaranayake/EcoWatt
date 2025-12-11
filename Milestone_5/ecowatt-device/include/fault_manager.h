#pragma once
#include <Arduino.h>

// Error type enumeration
enum class ErrorType {
  NONE = 0,
  HTTP_FAIL,
  MODBUS_EXCEPTION,
  CRC_ERROR,
  CORRUPT_FRAME,
  TIMEOUT,
  PACKET_DROP,
  UNKNOWN
};

typedef struct {
  uint32_t ts_ms;
  char code[24];
  char details[128];
  ErrorType error_type;
} FaultEvent;

void fault_init();
void fault_log(const char* code, const char* details);
void fault_log_typed(const char* code, const char* details, ErrorType type);
size_t fault_get_recent(FaultEvent* out, size_t max_items);
void fault_persist();
void fault_log_recovery(const char* code, const char* module);


// Error detection and classification helpers
ErrorType classify_error(const char* code);
bool is_retryable_error(ErrorType type);
const char* error_type_to_string(ErrorType type);
