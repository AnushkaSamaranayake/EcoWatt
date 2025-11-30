#ifndef BUFFER_H
#define BUFFER_H

#include <Arduino.h>

#include "acquisition_scheduler.h"

void buffer_init(size_t capacity);
bool buffer_push(const Sample& s);
bool buffer_push_with_log(const Sample& s);
size_t buffer_count();
size_t buffer_capacity();
void buffer_drain_all(Sample* out_array, size_t& out_count); // copy & clear

void buffer_clear(); // clear without reading

#endif
