#pragma once
#include <Arduino.h>

namespace FOTA {
  void run(const char* apiBase, const char* currentVersion);
}