#include "fota_simple.h"
#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <Updater.h>

namespace FOTA_SIMPLE {

void run(const char* apiBase) {
  Serial.println("[FOTA_SIMPLE] Checking for firmware...");

  WiFiClient client;
  HTTPClient http;

  String url = String(apiBase) + "/simple_fota";
  if (!http.begin(client, url)) {
    Serial.println("[FOTA_SIMPLE] http.begin failed");
    return;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[FOTA_SIMPLE] No update (%d)\n", code);
    http.end();
    return;
  }

  int contentLength = http.getSize();
  Serial.printf("[FOTA_SIMPLE] Firmware size: %d bytes\n", contentLength);

  if (contentLength <= 0) {
    Serial.println("[FOTA_SIMPLE] Invalid content length");
    http.end();
    return;
  }

  if (!Update.begin(contentLength)) {
    Serial.printf("[FOTA_SIMPLE] Update.begin failed: %s\n",
                  Update.getErrorString().c_str());
    http.end();
    return;
  }

  WiFiClient* stream = http.getStreamPtr();

if (!stream || !stream->available()) {
  Serial.println("[FOTA_SIMPLE] Stream not available");
  Update.end(false);
  http.end();
  return;
}

size_t written = Update.writeStream(*stream);

  if (written != contentLength) {
    Serial.printf("[FOTA_SIMPLE] Write failed (%d/%d)\n",
                  written, contentLength);
    Update.end(false);
    http.end();
    return;
  }

  if (!Update.end(true)) {
    Serial.printf("[FOTA_SIMPLE] Update.end failed: %s\n",
                  Update.getErrorString().c_str());
    http.end();
    return;
  }

  Serial.println("[FOTA_SIMPLE] Update successful. Rebooting...");
  http.end();
  ESP.restart();
}

}
