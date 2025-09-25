#ifndef PACKETIZER_H
#define PACKETIZER_H

#include <Arduino.h>
#include "buffer.h"

bool finalize_and_upload(const char* device_id, unsigned long interval_start_ms, uint8_t* workbuf, size_t workcap);

#endif