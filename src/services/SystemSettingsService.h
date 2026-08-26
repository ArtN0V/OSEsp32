#pragma once

#include <Arduino.h>

enum class SystemLanguage : uint8_t {
  English = 0,
  Russian = 1,
};

class SystemSettingsService {
 public:
  uint8_t loadBrightness() const;
  bool saveBrightness(uint8_t brightness) const;
  bool loadRotation180() const;
  bool saveRotation180(bool enabled) const;
  bool loadWallpaper(char* path, size_t pathSize) const;
  bool saveWallpaper(const char* path) const;
  bool clearWallpaper() const;
  SystemLanguage loadLanguage() const;
  bool saveLanguage(SystemLanguage language) const;
};
