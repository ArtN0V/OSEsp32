// PlatformIO compatibility entry point. Arduino IDE ignores this block and
// uses OperationSystem.ino, so there is still exactly one setup()/loop().
#ifdef PLATFORMIO

#include "diagnostics/DiagnosticsApp.h"

DiagnosticsApp diagnostics;

void setup() {
  diagnostics.begin();
}

void loop() {
  diagnostics.update();
}

#endif
