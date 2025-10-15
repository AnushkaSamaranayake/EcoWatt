#include "fota.h"
#include <FS.h>

#include <EEPROM.h>
#include <Arduino.h>

#define FOTA_STATE_ADDR 512
#define TEMP_PATH "/fota_tmp.bin"
#define EEPROM_SIZE 4096

static FotaManifest current_manifest;
static uint32_t next_chunk = 0;
static bool in_progress = false;

void fota_init() {
  SPIFFS.begin();
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(FOTA_STATE_ADDR, next_chunk);
  if (next_chunk == 0xFFFFFFFF) next_chunk = 0;
  in_progress = false;
}

bool fota_start(const FotaManifest& manifest) {
  current_manifest = manifest;
  next_chunk = 0;
  in_progress = true;
  if (SPIFFS.exists(TEMP_PATH)) SPIFFS.remove(TEMP_PATH);
  EEPROM.put(FOTA_STATE_ADDR, next_chunk);
  EEPROM.commit();
  return true;
}

uint32_t fota_next_expected_chunk() { return next_chunk; }

bool fota_handle_chunk(uint32_t chunk_number, const uint8_t* data, size_t len) {
  if (!in_progress) return false;
  if (chunk_number < next_chunk) return true; // duplicate accepted
  if (chunk_number != next_chunk) return false;
  File f = SPIFFS.open(TEMP_PATH, FILE_APPEND);
  if (!f) return false;
  size_t wrote = f.write(data, len);
  f.close();
  if (wrote != len) return false;
  next_chunk++;
  EEPROM.put(FOTA_STATE_ADDR, next_chunk);
  EEPROM.commit();
  return true;
}

bool fota_finalize(String& reason_out) {
  if (!in_progress) { reason_out = "no_in_progress"; return false; }
  File f = SPIFFS.open(TEMP_PATH, FILE_READ);
  if (!f) { reason_out = "missing"; return false; }
  uint32_t size = f.size();
  if (size != current_manifest.size) { f.close(); reason_out = "size_mismatch"; return false; }
  // TODO: compute SHA-256 and compare to manifest.hash_hex
  // For demo assume OK
  f.close();
  // simulate apply: rename temp to firmware file
  String newpath = String("/firmware_") + current_manifest.version + ".bin";
  if (SPIFFS.exists(newpath)) SPIFFS.remove(newpath);
  SPIFFS.rename(TEMP_PATH, newpath);
  next_chunk = 0;
  EEPROM.put(FOTA_STATE_ADDR, next_chunk);
  EEPROM.commit();
  in_progress = false;
  reason_out = "ok";
  return true;
}