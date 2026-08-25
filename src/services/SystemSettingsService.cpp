#include "SystemSettingsService.h"

#include <Preferences.h>

namespace {
constexpr const char* NVS_NAMESPACE = "osesp32_cfg";
constexpr const char* BRIGHTNESS_KEY = "brightness";
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
