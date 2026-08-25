#include "OSEsp32App.h"

void OSEsp32App::begin() {
  Serial.begin(115200);
  delay(250);
  if (!kernel_.begin()) {
    Serial.println("[FATAL] System kernel initialization failed");
  }
  diagnostics_.begin(kernel_);
}

void OSEsp32App::update() {
  kernel_.update();
  diagnostics_.update();
}
