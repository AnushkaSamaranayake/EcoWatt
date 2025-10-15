#include "acquisition_scheduler.h"
#include "protocol_adapter.h"
#include "buffer.h"
#include "packetizer.h"



unsigned long lastPoll = 0;
unsigned long lastUpload=0;
unsigned long POLL_INTERVAL_MS = 1000;
unsigned long UPLOAD_INTERVAL_MS = 15000; // demo 15s instead of 15min
extern const char* DEVICE_ID;

float gainForRegister(uint16_t reg) {
  switch (reg) {
    case 0: return 10.0f; // Vac1
    case 1: return 10.0f; // Iac1
    default: return 1.0f;
  }
}





void acquireOnce() {
  unsigned long now = millis();
  if (now - lastPoll >= POLL_INTERVAL_MS) {
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

  if (now - lastUpload >= UPLOAD_INTERVAL_MS){
    lastUpload = now;
    uint8_t workbuf[2048];
    bool ok = finalize_and_upload(DEVICE_ID, (unsigned long)(lastUpload), workbuf, sizeof(workbuf));
    if (!ok) Serial.println("[UPLOAD] failed");
  }


//  if (bufCount % 3 == 0) {
//    writeSingleRegister(0x0008, 50); // export power 50%
//    Serial.println("Export power set to 50%");
//  }
}

void schedulerLoop() {

    acquireOnce();
  
}
