#include "buffer.h"

// Ring buffer storage for samples
static Sample* ring = nullptr;   // Pointer to dynamically allocated array of Sample objects
static size_t head = 0;          // Index of the first element in the buffer
static size_t count = 0;         // Current number of elements stored
static size_t cap = 0;           // Maximum capacity of the buffer

void buffer_init(size_t capacity) {
  if (ring) free(ring); // Free old memory if already allocated
  ring = (Sample*)malloc(sizeof(Sample)*capacity); // Allocate new memory for Samples
  head = 0; 
  count = 0; 
  cap = capacity;
}


//push a new sample to the buffer
bool buffer_push(const Sample& s) {
  if (count >= cap) return false; // Buffer is full, reject insert
  size_t idx = (head + count) % cap; // Compute next insertion index in circular buffer
  ring[idx] = s; // Copy sample into buffer
  count++;       // Increase number of items
  return true;
}

size_t buffer_count(){ return count; }
size_t buffer_capacity(){ return cap; }

void buffer_drain_all(Sample* out_array, size_t& out_count) {
  out_count = 0;
  for (size_t i=0;i<count;i++) {
    size_t idx = (head + i) % cap;  // Compute actual index in circular buffer
    out_array[i] = ring[idx];       // Copy sample to output array
    out_count++;
  }
  // Reset buffer state (clear all data)
  head = 0;
  count = 0;
}