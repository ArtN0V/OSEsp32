#include "SystemSettingsService.h"

#include <Preferences.h>

namespace {
constexpr const char* NVS_NAMESPACE = "osesp32_cfg";
constexpr const char* BRIGHTNESS_KEY = "brightness";
constexpr const char* ROTATION_KEY = "rotate180";
constexpr const char* WALLPAPER_KEY = "wallpaper";
constexpr const char* DESKTOP_COLOR_KEY = "desk_color";
constexpr const char* LANGUAGE_KEY = "language";
constexpr const char* CLOCK_UTC_KEY = "clock_utc";
constexpr const char* CLOCK_ZONE_KEY = "clock_zone";
constexpr const char* SCREEN_SAVER_ENABLED_KEY = "ss_enabled";
constexpr const char* SCREEN_SAVER_TIMEOUT_KEY = "ss_timeout";
constexpr const char* SCREEN_SAVER_IMAGE_KEY = "ss_image";
constexpr uint8_t DEFAULT_BRIGHTNESS = 255;
constexpr uint8_t MINIMUM_BRIGHTNESS = 25;
}

uint8_t SystemSettingsService::loadBrightness() const {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, true)) return DEFAULT_BRIGHTNESS;
  uint8_t value = preferences.getUChar(BRIGHTNESS_KEY, DEFAULT_BRIGHTNESS);
  preferences.end();
  return value < MINIMUM_BRIGHTNESS ? MINIMUM_BRIGHTNESS : value;
}

bool SystemSettingsService::saveBrightness(uint8_t brightness) const {
  if (brightness < MINIMUM_BRIGHTNESS) brightness = MINIMUM_BRIGHTNESS;
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;
  const bool saved =
      preferences.putUChar(BRIGHTNESS_KEY, brightness) == sizeof(uint8_t);
  preferences.end();
  return saved;
}

bool SystemSettingsService::loadRotation180() const {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, true)) return false;
  const bool enabled = preferences.getBool(ROTATION_KEY, false);
  preferences.end();
  return enabled;
}

bool SystemSettingsService::saveRotation180(bool enabled) const {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;
  const bool saved =
      preferences.putBool(ROTATION_KEY, enabled) == sizeof(bool);
  preferences.end();
  return saved;
}

bool SystemSettingsService::loadWallpaper(char* path, size_t pathSize) const {
  if (!path || pathSize == 0) return false;
  path[0] = '\0';
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, true)) return false;
  const size_t length = preferences.getString(WALLPAPER_KEY, path, pathSize);
  preferences.end();
  return length > 0 && length < pathSize;
}

bool SystemSettingsService::saveWallpaper(const char* path) const {
  if (!path || !path[0]) return false;
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;
  const size_t length = strlen(path);
  const bool saved = preferences.putString(WALLPAPER_KEY, path) == length;
  preferences.end();
  return saved;
}

bool SystemSettingsService::clearWallpaper() const {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;
  const bool removed = !preferences.isKey(WALLPAPER_KEY) ||
                       preferences.remove(WALLPAPER_KEY);
  preferences.end();
  return removed;
}

uint8_t SystemSettingsService::loadDesktopColor() const {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, true)) return 0;
  const uint8_t value = preferences.getUChar(DESKTOP_COLOR_KEY, 0);
  preferences.end();
  return value;
}

bool SystemSettingsService::saveDesktopColor(uint8_t colorIndex) const {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;
  const bool saved =
      preferences.putUChar(DESKTOP_COLOR_KEY, colorIndex) == sizeof(uint8_t);
  preferences.end();
  return saved;
}

bool SystemSettingsService::loadClock(uint64_t& utcSeconds,
                                      int16_t& timezoneMinutes) const {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, true)) return false;
  if (!preferences.isKey(CLOCK_UTC_KEY)) {
    preferences.end();
    return false;
  }
  utcSeconds = preferences.getULong64(CLOCK_UTC_KEY, 0);
  timezoneMinutes = static_cast<int16_t>(
      preferences.getShort(CLOCK_ZONE_KEY, 180));
  preferences.end();
  return utcSeconds > 0;
}

bool SystemSettingsService::saveClock(uint64_t utcSeconds,
                                      int16_t timezoneMinutes) const {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;
  const bool savedUtc =
      preferences.putULong64(CLOCK_UTC_KEY, utcSeconds) == sizeof(uint64_t);
  const bool savedZone = preferences.putShort(CLOCK_ZONE_KEY,
                                               timezoneMinutes) ==
                         sizeof(int16_t);
  preferences.end();
  return savedUtc && savedZone;
}

bool SystemSettingsService::loadScreenSaverEnabled() const {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, true)) return false;
  const bool enabled = preferences.getBool(SCREEN_SAVER_ENABLED_KEY, false);
  preferences.end();
  return enabled;
}

bool SystemSettingsService::saveScreenSaverEnabled(bool enabled) const {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;
  const bool saved = preferences.putBool(SCREEN_SAVER_ENABLED_KEY, enabled) ==
                     sizeof(bool);
  preferences.end();
  return saved;
}

uint8_t SystemSettingsService::loadScreenSaverTimeout() const {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, true)) return 2;
  const uint8_t value = preferences.getUChar(SCREEN_SAVER_TIMEOUT_KEY, 2);
  preferences.end();
  return value;
}

bool SystemSettingsService::saveScreenSaverTimeout(uint8_t timeoutIndex) const {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;
  const bool saved = preferences.putUChar(SCREEN_SAVER_TIMEOUT_KEY,
                                           timeoutIndex) == sizeof(uint8_t);
  preferences.end();
  return saved;
}

bool SystemSettingsService::loadScreenSaverImage(char* path,
                                                  size_t pathSize) const {
  if (!path || !pathSize) return false;
  path[0] = '\0';
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, true)) return false;
  const size_t length =
      preferences.getString(SCREEN_SAVER_IMAGE_KEY, path, pathSize);
  preferences.end();
  return length > 0 && length < pathSize;
}

bool SystemSettingsService::saveScreenSaverImage(const char* path) const {
  if (!path || !path[0]) return false;
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;
  const size_t length = strlen(path);
  const bool saved =
      preferences.putString(SCREEN_SAVER_IMAGE_KEY, path) == length;
  preferences.end();
  return saved;
}

bool SystemSettingsService::clearScreenSaverImage() const {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;
  const bool removed = !preferences.isKey(SCREEN_SAVER_IMAGE_KEY) ||
                       preferences.remove(SCREEN_SAVER_IMAGE_KEY);
  preferences.end();
  return removed;
}

SystemLanguage SystemSettingsService::loadLanguage() const {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, true)) return SystemLanguage::English;
  const uint8_t value = preferences.getUChar(
      LANGUAGE_KEY, static_cast<uint8_t>(SystemLanguage::English));
  preferences.end();
  return value == static_cast<uint8_t>(SystemLanguage::Russian)
             ? SystemLanguage::Russian
             : SystemLanguage::English;
}

bool SystemSettingsService::saveLanguage(SystemLanguage language) const {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;
  const bool saved = preferences.putUChar(LANGUAGE_KEY,
      static_cast<uint8_t>(language)) == sizeof(uint8_t);
  preferences.end();
  return saved;
}
