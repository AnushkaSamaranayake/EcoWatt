#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "protocol_adapter.h"
#include "acquisition_scheduler.h"
#include "buffer.h"
#include "packetizer.h"
#include <LittleFS.h>
#include "fota.h"
#include "cloud_sync.h"

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
const char* URL_UPLOAD  = "http://10.109.27.251:5000/api/ecowatt/cloud/upload";

const char* API_BASE = "http://10.188.60.251:5000";

const char* CURRENT_VERSION = "v1.0.0";


void connectWiFi() {
  Serial.printf("Connecting to %s...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");

  if (WiFi.status() == WL_CONNECTED) {
    FOTA::run(API_BASE,CURRENT_VERSION);
  } else {
    Serial.println("[FOTA] skipped (no WiFi)");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  LittleFS.begin();
  connectWiFi();
  buffer_init(256); // capacity (adjust)

}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  schedulerLoop();
}
