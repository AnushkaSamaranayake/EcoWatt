// power_manager.cpp - minimal, safe power management helpers
#include "power_manager.h"
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WiFiType.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFiAP.h>
#include <ESP.h>
extern "C" {
  #include "user_interface.h"
}
#endif

void power_init(){
  // noop for now; placeholder for future calibration
}

void power_set_cpu_freq(uint8_t mhz){
#ifdef ESP8266
  if (mhz==80) {
    system_update_cpu_freq(SYS_CPU_80MHZ);
  } else if (mhz==160) {
    system_update_cpu_freq(SYS_CPU_160MHZ);
  }
#endif
}

void power_idle_sleep_hint_ms(uint32_t ms){
  // A cooperative hint: if ms is large, lower CPU frequency briefly
#ifdef ESP8266
  if (ms >= 200) {
    // drop to 80 MHz briefly to save power
    uint8_t cur = system_get_cpu_freq();
    if (cur > 80) system_update_cpu_freq(SYS_CPU_80MHZ);
    delay(ms);
    // restore to 160 if needed
    if (cur > 80) system_update_cpu_freq(SYS_CPU_160MHZ);
  } else {
    // short hint: just delay
    delay(ms);
  }
#else
  delay(ms);
#endif
}

void power_enter_light_sleep(){
#ifdef ESP8266
  // Attempt to force WiFi light sleep - cooperative; may not always be beneficial
  WiFi.forceSleepBegin();
  delay(10);
  // caller must call WiFi.forceSleepWake() when done if needed
#endif
}
