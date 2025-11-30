// power_manager.h - simple power management helpers for ESP8266
#pragma once

#include <Arduino.h>

void power_init();
void power_set_cpu_freq(uint8_t mhz);
void power_idle_sleep_hint_ms(uint32_t ms);
void power_enter_light_sleep();
