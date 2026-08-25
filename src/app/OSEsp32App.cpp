#include "OSEsp32App.h"

void OSEsp32App::begin() {
  Serial.begin(115200);
  delay(250);
  if (!kernel_.begin()) {
    Serial.println("[FATAL] System kernel initialization failed");
  }
  activeMode_ = bootMode_.consumeRequestedMode();
  if (activeMode_ == BootMode::Diagnostics) {
    diagnostics_.begin(kernel_);
  } else if (!shell_.begin(kernel_, bootMode_)) {
    Serial.println("[FATAL] Graphical shell initialization failed");
    kernel_.setLifecycle(LifecycleState::SafeMode);
    diagnostics_.begin(kernel_);
    activeMode_ = BootMode::Diagnostics;
  }
}

void OSEsp32App::update() {
  kernel_.update();
  if (activeMode_ == BootMode::Diagnostics)
    diagnostics_.update();
  else
    shell_.update();
}
