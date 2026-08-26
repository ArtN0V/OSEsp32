#include "SystemSettingsService.h"

#include <Preferences.h>

namespace {
constexpr const char* NVS_NAMESPACE = "osesp32_cfg";
constexpr const char* BRIGHTNESS_KEY = "brightness";
constexpr const char* ROTATION_KEY = "rotate180";
constexpr const char* WALLPAPER_KEY = "wallpaper";
constexpr const char* DESKTOP_COLOR_KEY = "desk_color";
constexpr const char* LANGUAGE_KEY = "language";
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
