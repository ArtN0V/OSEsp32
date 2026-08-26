#pragma once

#include <Arduino.h>

class SystemSettingsService {
 public:
  uint8_t loadBrightness() const;
  bool saveBrightness(uint8_t brightness) const;
  bool loadRotation180() const;
  bool saveRotation180(bool enabled) const;
  bool loadWallpaper(char* path, size_t pathSize) const;
  bool saveWallpaper(const char* path) const;
  bool clearWallpaper() const;
};
