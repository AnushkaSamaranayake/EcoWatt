#ifndef ACQUISITION_SCHEDULER_H
#define ACQUISITION_SCHEDULER_H

#include <Arduino.h>

struct Sample {
  unsigned long ts;
  float voltage;
  float current;
};


void bufferPush(const Sample& s);
void acquireOnce();
void schedulerLoop();

#endif
