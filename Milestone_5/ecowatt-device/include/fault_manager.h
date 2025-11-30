#pragma once
#include <Arduino.h>

typedef struct {
  uint32_t ts_ms;
  char code[24];
  char details[128];
} FaultEvent;

void fault_init();
void fault_log(const char* code, const char* details);
size_t fault_get_recent(FaultEvent* out, size_t max_items);
void fault_persist();
