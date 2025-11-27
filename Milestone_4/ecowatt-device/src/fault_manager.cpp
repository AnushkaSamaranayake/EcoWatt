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

void fault_log(const char* code, const char* details){
  uint32_t now = millis();
  FaultEvent e;
  e.ts_ms = now;
  strncpy(e.code, code, sizeof(e.code)-1); e.code[sizeof(e.code)-1]=0;
  strncpy(e.details, details, sizeof(e.details)-1); e.details[sizeof(e.details)-1]=0;

  // store in circular buffer
  size_t idx = (ev_head + ev_count) % (sizeof(events)/sizeof(events[0]));
  events[idx] = e;
  if (ev_count < (sizeof(events)/sizeof(events[0]))) ev_count++; else ev_head = (ev_head+1) % (sizeof(events)/sizeof(events[0]));

  // also append to file for persistence
  File f = LittleFS.open("/fault_log.txt", "a");
  if (f) {
    f.printf("%lu,%s,%s\n", (unsigned long)e.ts_ms, e.code, e.details);
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
