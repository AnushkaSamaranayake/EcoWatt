#include <Arduino.h>

struct FotaManifest {
  String version;
  uint32_t size;
  String hash_hex;
  uint32_t chunk_size;
  uint32_t total_chunks;
};

void fota_init();
bool fota_start(const FotaManifest& manifest);
uint32_t fota_next_expected_chunk();
bool fota_handle_chunk(uint32_t chunk_number, const uint8_t* data, size_t len);
bool fota_finalize(String& reason_out);