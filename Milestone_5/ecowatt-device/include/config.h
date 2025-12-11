#ifndef CONFIG_H
#define CONFIG_H
#include <Arduino.h>  
#include <stdint.h>

struct AppConfig {
  uint32_t version;
  uint32_t sampling_interval_s;
  uint32_t upload_interval_s;
  uint8_t register_mask;
  uint16_t max_buffer_size;
};

void config_init();
bool config_stage_from_json(const String& json, String& accepted_json, String& rejected_json);
void config_apply_staged();
AppConfig config_get_active();
String config_build_ack_json();

#endif // CONFIG_H