#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "protocol_adapter.h"
#include "acquisition_scheduler.h"

const char* WIFI_SSID     = "OPPO_A3s";
const char* WIFI_PASSWORD = "12345678";

// Cloud API URLs
const char* URL_READ  = "http://20.15.114.131:8080/api/inverter/read";
const char* URL_WRITE = "http://20.15.114.131:8080/api/inverter/write";
const char* API_KEY   = "NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTIwOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNg==";

void connectWiFi() {
  Serial.printf("Connecting to %s...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
}

void setup() {
  Serial.begin(115200);
  connectWiFi();

}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  schedulerLoop();
}
