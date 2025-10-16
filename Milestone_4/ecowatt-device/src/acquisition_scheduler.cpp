#include "acquisition_scheduler.h"
#include "protocol_adapter.h"
#include "buffer.h"
#include "packetizer.h"
#include "config.h"
#include <Arduino.h>


unsigned long lastPoll = 0;
unsigned long lastUpload=0;
unsigned long POLL_INTERVAL_MS = 1000;
unsigned long UPLOAD_INTERVAL_MS = 15000; // demo 15s instead of 15min
const char* DEVICE_ID = "EcoWatt001";

float gainForRegister(uint16_t reg) {
  switch (reg) {
    case 0: return 10.0f; // Vac1
    case 1: return 10.0f; // Iac1
    default: return 1.0f;
  }
}





void acquireOnce() {
  AppConfig cfg = config_get_active();
  unsigned long poll_ms = cfg.sampling_interval_s * 1000UL;
  unsigned long now = millis();
  if (now - lastPoll >= poll_ms) {
    lastPoll = now;
    uint16_t rawV = 0, rawI = 0;
    bool okV = readRegisterU16(0x0000, 0x0001, rawV);
    bool okI = readRegisterU16(0x0001, 0x0001, rawI);

    float voltage = okV ? rawV / gainForRegister(0) : NAN;
    float current = okI ? rawI / gainForRegister(1) : NAN;

    Sample s = { millis(), voltage, current };
    bool ok = buffer_push(s);
    if (!ok) Serial.println("[BUFFER] overflow!");
    else Serial.printf("[SAMPLE] Voltage=%f Current=%f buffer=%d\n", voltage, current, buffer_count());
  }

  unsigned long up_ms = cfg.upload_interval_s * 1000UL;
  if (now - lastUpload >= up_ms){
    lastUpload = now;
    
    // Allocate work buffer on heap to prevent stack overflow
    const size_t workbuf_size = 2048;
    uint8_t* workbuf = (uint8_t*)malloc(workbuf_size);
    if (!workbuf) {
      Serial.println("[ERROR] Failed to allocate work buffer");
      return;
    }
    
    bool ok = finalize_and_upload(DEVICE_ID, (unsigned long)(lastUpload), workbuf, workbuf_size);
    free(workbuf); // Always free the buffer
    
    if (!ok) Serial.println("[UPLOAD] failed");
  }


//  if (bufCount % 3 == 0) {
//    writeSingleRegister(0x0008, 50); // export power 50%
//    Serial.println("Export power set to 50%");
//  }
}

void schedulerLoop() {


  config_apply_staged();
  acquireOnce();
  
}
