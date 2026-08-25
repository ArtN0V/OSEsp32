#pragma once

#include <Arduino.h>

class SystemSettingsService {
 public:
  uint8_t loadBrightness() const;
  bool saveBrightness(uint8_t brightness) const;
};
