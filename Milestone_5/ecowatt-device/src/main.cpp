#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "protocol_adapter.h"
#include "acquisition_scheduler.h"
#include "buffer.h"
#include "packetizer.h"
#include <LittleFS.h>
#include "fota.h"
#include "cloud_sync.h"
#include "version_store.h"
#include "power_manager.h"
#include "fault_manager.h"
#include "config.h"

const char* WIFI_SSID     = "Anushka's Galaxy M12";
const char* WIFI_PASSWORD = "12345678";

// Cloud API URLs
const char* URL_READ  = "http://20.15.114.131:8080/api/inverter/read";
const char* URL_WRITE = "http://20.15.114.131:8080/api/inverter/write";
const char* API_KEY   = "NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTIwOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNg==";
// const char* URL_UPLOAD = "http://10.109.27.251:5000/api/ecowatt/cloud/upload"; 
const char* API_KEY_1   = "EcowattUploadsMzo11quhRx40l6eqLV22BWvQ5ozk6iolLO60GaOa1j2R7tthoSiKHlpROjT5nW2179vkSeJoMQsjZWjLDLgVOIVM86SlaRki0cgWqep9QRHYlbqsPC";

// Replace with the PSK that matches Flask's API_KEYS
// const char* DEVICE_PSK = "aa1c38aaa59b94ff2339060e298826e2"; 
const char* URL_UPLOAD  = "http://10.28.177.230:5000/api/ecowatt/cloud/upload";

const char* API_BASE = "http://10.28.177.230:5000";

//const char* CURRENT_VERSION = "v1.0.0";

static uint32_t lastPowerPrint = 0;


void connectWiFi() {
  Serial.printf("Connecting to %s...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
}

void setup() {
  Serial.begin(115200);
  delay(100);
  LittleFS.begin();

  fault_init();
  power_init();
  config_init();

  version_init();
  String storedVersion = loadCurrentVersion();
  if (storedVersion.length() == 0 || storedVersion == "\xFF") {
    storedVersion = "1.0.0";
    saveCurrentVersion(storedVersion.c_str());
  }

  connectWiFi();

  // if (WiFi.status() == WL_CONNECTED) {
  //   FOTA::run(API_BASE, storedVersion.c_str());
  // } else {
  //   Serial.println("[FOTA] skipped (no WiFi)");
  // }

  Serial.println("Setup started");
  buffer_init(256); // capacity (adjust)
} 

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  schedulerLoop();
  // cooperative idle hint to reduce active CPU frequency briefly
  power_idle_sleep_hint_ms(50);

  if (millis() - lastPowerPrint > 60000) { // every 1 min
    PowerStats p = power_get_stats();

    Serial.println("===== POWER REPORT =====");
    Serial.printf("Uptime: %.2f min\n", p.uptime_ms / 60000.0);
    Serial.printf("Baseline Energy: %.3f mAh\n", p.baseline_mAh);
    Serial.printf("Optimized Energy: %.3f mAh\n", p.optimized_mAh);
    Serial.printf("Energy Saved: %.3f mAh\n", p.saved_mAh);
    Serial.printf("Power Saved: %.2f %%\n", p.saved_percent);
    Serial.println("========================");

    lastPowerPrint = millis();
}

}
