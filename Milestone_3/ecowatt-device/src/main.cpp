#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "protocol_adapter.h"
#include "acquisition_scheduler.h"
#include "buffer.h"
#include "packetizer.h"
#include "compressor.h"

const char* WIFI_SSID     = "Anushka's Galaxy M12";
const char* WIFI_PASSWORD = "12345678";

// Cloud API URLs
const char* URL_READ  = "http://20.15.114.131:8080/api/inverter/read";
const char* URL_WRITE = "http://20.15.114.131:8080/api/inverter/write";
const char* API_KEY   = "NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YTIwOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWExNg==";
const char* URL_UPLOAD = "http://10.104.204.251:5000/api/ecowatt/cloud/upload";
const char* DEVICE_ID = "EcoWatt001";
const char* API_KEY_EW   = "EcowattUploadsMzo11quhRx40l6eqLV22BWvQ5ozk6iolLO60GaOa1j2R7tthoSiKHlpROjT5nW2179vkSeJoMQsjZWjLDLgVOIVM86SlaRki0cgWqep9QRHYlbqsPC";

#define OUT_CAP 1024  

void connectWiFi() {
  Serial.printf("Connecting to %s...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
}

void run_benchmark(size_t n){
  Sample *samples = new Sample[n];
  uint8_t *out_buf = new uint8_t[OUT_CAP];

  for (size_t i=0; i<n; i++){
    samples[i].voltage = 230 + (i % 5); // 230.0V to 234.0V
    samples[i].current = 10 + (i % 3);  // 10.0A to 12.0A
  }

  // Benchmark compression
  unsigned long t1 = millis();
  size_t compressed_size = compress_delta_rle(samples,n,out_buf,OUT_CAP);
  unsigned long t2 = micros();
  size_t raw_size = n * sizeof(Sample);

  Serial.printf("\n=====Benchmark for %d samples=====\n", (int)n);
  Serial.printf("Raw size: %d bytes\n", raw_size);
  Serial.printf("Compressed size: %d bytes\n", compressed_size);
  Serial.printf("Compression ratio: %.2fx\n", (float)raw_size / compressed_size);
  Serial.printf("Encode time: %lu us\n", t2 - t1);

  delete[] samples;
  delete[] out_buf;
}

void setup() {
  Serial.begin(115200);
  connectWiFi();
  buffer_init(256); // capacity (adjust)

  // Run tests for different buffer sizes
  run_benchmark(50);
  run_benchmark(100);
  run_benchmark(500);
  run_benchmark(1000);

}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  schedulerLoop();
}
