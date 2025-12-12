#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include <Arduino.h>
#include "buffer.h"
#include "acquisition_scheduler.h"

// Encoded binary buffer storage (caller provides storage)
size_t compress_delta_rle(const Sample* samples, size_t n, uint8_t* out_buf, size_t out_cap);
bool decompress_delta_rle(const uint8_t* in_buf, size_t in_len, Sample* out_samples, size_t& out_n);

size_t compress_delta_rle_v2(
  const Sample* samples,
  size_t n,
  uint8_t channel_mask,
  uint8_t* out,
  size_t cap
);

#endif
