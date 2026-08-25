#include "BootModeService.h"

#include <Preferences.h>

namespace {
constexpr const char* NVS_NAMESPACE = "osesp32_boot";
constexpr const char* NEXT_MODE_KEY = "next";
}

BootMode BootModeService::consumeRequestedMode() {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, false)) return BootMode::Shell;
  const uint8_t requested = preferences.getUChar(
      NEXT_MODE_KEY, static_cast<uint8_t>(BootMode::Shell));
  preferences.remove(NEXT_MODE_KEY);
  preferences.end();
  return requested == static_cast<uint8_t>(BootMode::Diagnostics)
             ? BootMode::Diagnostics
             : BootMode::Shell;
}

bool BootModeService::requestDiagnostics() {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;
  const bool saved = preferences.putUChar(
                         NEXT_MODE_KEY,
                         static_cast<uint8_t>(BootMode::Diagnostics)) ==
                     sizeof(uint8_t);
  preferences.end();
  return saved;
}
