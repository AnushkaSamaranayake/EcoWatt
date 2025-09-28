#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "protocol_adapter.h"
#include "acquisition_scheduler.h"
#include "buffer.h"
#include "packetizer.h"

const char* WIFI_SSID     = "Anushka_4G";
const char* WIFI_PASSWORD = "Mass@210559H";

// Cloud API URLs
const char* URL_READ  = "http://20.15.114.131:8080/api/inverter/read";
const char* URL_WRITE = "http://20.15.114.131:8080/api/inverter/write";
const char* API_KEY   = "NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTIwOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNg==";
const char* URL_UPLOAD = "http://192.168.8.100:5000/api/ecowatt/cloud/upload";
const char* DEVICE_ID = "EcoWatt001";
const char* API_KEY_EW   = "EcowattUploadsMzo11quhRx40l6eqLV22BWvQ5ozk6iolLO60GaOa1j2R7tthoSiKHlpROjT5nW2179vkSeJoMQsjZWjLDLgVOIVM86SlaRki0cgWqep9QRHYlbqsPC";


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
  buffer_init(256); // capacity (adjust)

}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  schedulerLoop();
}
