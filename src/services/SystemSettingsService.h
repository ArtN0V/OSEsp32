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
  uint8_t loadDesktopColor() const;
  bool saveDesktopColor(uint8_t colorIndex) const;
  bool loadClock(uint64_t& utcSeconds, int16_t& timezoneMinutes) const;
  bool saveClock(uint64_t utcSeconds, int16_t timezoneMinutes) const;
  bool loadScreenSaverEnabled() const;
  bool saveScreenSaverEnabled(bool enabled) const;
  uint8_t loadScreenSaverTimeout() const;
  bool saveScreenSaverTimeout(uint8_t timeoutIndex) const;
  bool loadScreenSaverImage(char* path, size_t pathSize) const;
  bool saveScreenSaverImage(const char* path) const;
  bool clearScreenSaverImage() const;
  SystemLanguage loadLanguage() const;
  bool saveLanguage(SystemLanguage language) const;
};
