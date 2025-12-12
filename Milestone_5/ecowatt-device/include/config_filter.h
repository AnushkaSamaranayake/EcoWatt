// config_filter.h
#pragma once

#include "config.h"

// These helpers look at AppConfig.register_mask
// bit 0 -> voltage
// bit 1 -> current
// bit 2 -> frequency (reserved / future use)

bool should_read_voltage();
bool should_read_current();
bool should_read_frequency();
bool should_read_temperature();
