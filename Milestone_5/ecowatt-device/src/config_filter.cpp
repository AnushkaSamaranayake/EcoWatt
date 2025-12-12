// config_filter.cpp
#include "config_filter.h"

// NOTE: config_get_active() is defined in config.cpp :contentReference[oaicite:0]{index=0}
// This module simply *reads* the active config and never writes it,
// so it cannot create recursion/loops.

// bool should_read_voltage() {
//     AppConfig cfg = config_get_active();
//     return (cfg.register_mask & (1u << 0)) != 0;
// }

// bool should_read_current() {
//     AppConfig cfg = config_get_active();
//     return (cfg.register_mask & (1u << 1)) != 0;
// }

// bool should_read_frequency() {
//     AppConfig cfg = config_get_active();
//     return (cfg.register_mask & (1u << 2)) != 0;
// }

bool should_read_voltage() {
  return config_get_active().register_mask & (1 << 0);
}

bool should_read_current() {
  return config_get_active().register_mask & (1 << 1);
}

bool should_read_frequency() {
  return config_get_active().register_mask & (1 << 2);
}

bool should_read_temperature() {
  return config_get_active().register_mask & (1 << 3);
}


