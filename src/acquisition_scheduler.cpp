#include "acquisition_scheduler.h"
#include "protocol_adapter.h"

const size_t BUFFER_SIZE = 32;
static Sample bufferSamples[BUFFER_SIZE];
static size_t bufHead = 0;
static size_t bufCount = 0;

unsigned long lastPoll = 0;
unsigned long POLL_INTERVAL_MS = 5000;

float gainForRegister(uint16_t reg) {
  switch (reg) {
    case 0: return 10.0f; // Vac1
    case 1: return 10.0f; // Iac1
    default: return 1.0f;
  }
}

void bufferPush(const Sample& s) {
  bufferSamples[bufHead] = s;
  bufHead = (bufHead + 1) % BUFFER_SIZE;
  if (bufCount < BUFFER_SIZE) bufCount++;
}

void acquireOnce() {
  std::vector<uint16_t> values;
  if (readRegisterU16(0x0000, 0x0002, values, 3)) {
    // Expecting 2 values: [Vac1, Iac1]
    float voltage = values.size() > 0 ? values[0] / gainForRegister(0) : NAN;
    float current = values.size() > 1 ? values[1] / gainForRegister(1) : NAN;

    Sample s = { millis(), voltage, current };
    bufferPush(s);

    Serial.printf("Voltage=%.1fV, Current=%.1fA\n", voltage, current);

    // Example: write every 3rd sample
    if (bufCount % 3 == 0) {
      if (writeSingleRegister(0x0008, 50, 3)) {
        Serial.println("Export power set to 50%");
      } else {
        Serial.println("Failed to write export power");
      }
    }
  } else {
    Serial.println("Failed to read registers 0 and 1");
  }
}

void schedulerLoop() {
  unsigned long now = millis();
  if (now - lastPoll >= POLL_INTERVAL_MS) {
    lastPoll = now;
    acquireOnce();
  }
}
