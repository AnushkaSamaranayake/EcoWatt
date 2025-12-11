#include "power_manager.h"
#ifdef ESP8266
#include <ESP8266WiFi.h>
extern "C" {
  #include "user_interface.h"
}
#endif
#include <Arduino.h>

// ==========================
// Internal tracking variables
// ==========================
static uint64_t cpu80_time = 0;
static uint64_t cpu160_time = 0;
static uint32_t last_freq_switch = 0;
static uint8_t current_freq = 160;

static uint64_t wifi_active_time = 0;
static uint32_t wifi_start_time = 0;
static bool wifi_is_active = true;

// ==========================
// Helpers to track CPU usage
// ==========================
static void record_freq_change(uint8_t new_mhz) {
    uint32_t now = millis();
    if (current_freq == 80) cpu80_time += now - last_freq_switch;
    if (current_freq == 160) cpu160_time += now - last_freq_switch;

    current_freq = new_mhz;
    last_freq_switch = now;
}

// ======================
// Initialize power system
// ======================
void power_init(){
  last_freq_switch = millis();

  // Enable modem sleep by default
#ifdef ESP8266
  wifi_set_sleep_type(MODEM_SLEEP_T);
#endif
}

// ======================
// CPU frequency control
// ======================
void power_set_cpu_freq(uint8_t mhz){
#ifdef ESP8266
  record_freq_change(mhz);
  if (mhz == 80) system_update_cpu_freq(SYS_CPU_80MHZ);
  if (mhz == 160) system_update_cpu_freq(SYS_CPU_160MHZ);
#endif
}

// ======================
// Idle hint for scheduler
// ======================
void power_idle_sleep_hint_ms(uint32_t ms){
#ifdef ESP8266
  if (ms >= 200) {
    power_set_cpu_freq(80);
    delay(ms);
    power_set_cpu_freq(160);
  } else {
    delay(ms);
  }
#else
  delay(ms);
#endif
}

// ====================================
// WiFi sleep/active time measurement
// ====================================
void power_wifi_active_start() {
    wifi_start_time = millis();
    wifi_is_active = true;
}

void power_wifi_active_end() {
    if (!wifi_is_active) return;
    wifi_active_time += millis() - wifi_start_time;
    wifi_is_active = false;
}

// ===================
// Enter light sleep
// ===================
void power_enter_light_sleep() {
#ifdef ESP8266
  WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
#endif
}

// ===================
// Energy estimation
// ===================
float power_estimate_current_mA() {
    float cpu80_mA = 55;
    float cpu160_mA = 75;
    float wifi_active_mA = 120; // average during HTTP traffic
    float wifi_idle_mA = 20;    // modem sleep

    float total_ms = millis();
    if (total_ms < 1000) return 0;

    float cpu_current =
        ((cpu80_time * cpu80_mA) + (cpu160_time * cpu160_mA)) / total_ms;

    float wifi_current =
        ((wifi_active_time * wifi_active_mA) +
        ((total_ms - wifi_active_time) * wifi_idle_mA)) /
        total_ms;

    return cpu_current + wifi_current;
}

// Debug reporter
void power_debug_report() {
    Serial.printf("[POWER] CPU80=%llu ms, CPU160=%llu ms\n", cpu80_time, cpu160_time);
    Serial.printf("[POWER] WiFiActive=%llu ms\n", wifi_active_time);
    Serial.printf("[POWER] Estimated current draw = %.2f mA\n", power_estimate_current_mA());
}
