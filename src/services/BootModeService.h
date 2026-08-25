#pragma once

#include <Arduino.h>

enum class BootMode : uint8_t { Shell = 0, Diagnostics = 1 };

class BootModeService {
 public:
  BootMode consumeRequestedMode();
  bool requestDiagnostics();
};
