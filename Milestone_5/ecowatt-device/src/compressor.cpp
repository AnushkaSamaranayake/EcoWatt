#include "compressor.h"
// --- Helper to write 16-bit big-endian values into a byte buffer ---
static inline void write_be16(uint8_t* p, uint16_t v) {
    p[0] = (v >> 8) & 0xFF;
    p[1] = v & 0xFF;
}

size_t compress_delta_rle(const Sample* samples, size_t n, uint8_t* out_buf, size_t out_cap) {
    if (!samples || !out_buf || n == 0 || out_cap < 6) return 0;
    
    // Limit maximum samples to prevent excessive memory usage
    if (n > 500) {
      Serial.println("[WARN] Too many samples, limiting to 500");
      n = 500;
    }

    size_t pos = 0;

    // --- Write magic header "DRL1" ---
    if (pos + 4 > out_cap) return 0;
    out_buf[pos++] = 'D';
    out_buf[pos++] = 'R';
    out_buf[pos++] = 'L';
    out_buf[pos++] = '1';

    // --- Write sample count (2 bytes, big-endian) ---
    if (pos + 2 > out_cap) return 0;
    write_be16(out_buf + pos, (uint16_t)n);
    pos += 2;

    // --- Encode voltage series ---
    int32_t prev = samples[0].voltage;

    // Write first voltage value
    if (pos + 2 > out_cap) return 0;
    write_be16(out_buf + pos, (uint16_t)prev);
    pos += 2;

    // Write deltas for all subsequent voltage samples
    for (size_t i = 1; i < n; i++) {
        int delta = (int)samples[i].voltage - (int)prev;
        prev = samples[i].voltage;

        if (pos + 1 > out_cap) return 0;

        if (delta >= -127 && delta <= 127) {
            // Small delta: store as signed byte
            out_buf[pos++] = (uint8_t)((int8_t)delta);
        } else {
            // Large delta: store marker + int16 value
            if (pos + 3 > out_cap) return 0;
            out_buf[pos++] = 0x7F; // marker
            write_be16(out_buf + pos, (uint16_t)delta);
            pos += 2;
        }
    }

    // --- Encode current series (same scheme as voltage) ---
    prev = samples[0].current;

    // Write first current value
    if (pos + 2 > out_cap) return 0;
    write_be16(out_buf + pos, (uint16_t)prev);
    pos += 2;

    // Write deltas for all subsequent current samples
    for (size_t i = 1; i < n; i++) {
        int delta = (int)samples[i].current - (int)prev;
        prev = samples[i].current;

        if (pos + 1 > out_cap) return 0;

        if (delta >= -127 && delta <= 127) {
            out_buf[pos++] = (uint8_t)((int8_t)delta);
        } else {
            if (pos + 3 > out_cap) return 0;
            out_buf[pos++] = 0x7F;
            write_be16(out_buf + pos, (uint16_t)delta);
            pos += 2;
        }
    }

    return pos; // total compressed size
}

size_t compress_delta_rle_v2(
  const Sample* s,
  size_t n,
  uint8_t mask,
  uint8_t* out,
  size_t cap
) {
  if (!s || n == 0 || cap < 8) return 0;
  size_t pos = 0;

  // Header
  out[pos++] = 'D'; out[pos++] = 'R';
  out[pos++] = 'L'; out[pos++] = '2';
  out[pos++] = mask;
  out[pos++] = (n >> 8) & 0xFF;
  out[pos++] = n & 0xFF;

  auto encode = [&](auto getter) {
    int16_t prev = getter(s[0]);
    out[pos++] = prev >> 8;
    out[pos++] = prev & 0xFF;
    for (size_t i = 1; i < n; i++) {
      int d = getter(s[i]) - prev;
      prev += d;
      if (d >= -127 && d <= 127) {
        out[pos++] = (int8_t)d;
      } else {
        out[pos++] = 0x7F;
        out[pos++] = d >> 8;
        out[pos++] = d & 0xFF;
      }
    }
  };

  if (mask & (1<<0)) encode([](const Sample& x){ return (int16_t)(x.voltage*10); });
  if (mask & (1<<1)) encode([](const Sample& x){ return (int16_t)(x.current*10); });
  if (mask & (1<<2)) encode([](const Sample& x){ return (int16_t)(x.frequency*100); });
  if (mask & (1<<3)) encode([](const Sample& x){ return (int16_t)(x.temperature*10); });

  return pos;
}
