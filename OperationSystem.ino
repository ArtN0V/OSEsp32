#include "src/diagnostics/DiagnosticsApp.h"

DiagnosticsApp diagnostics;

void setup() {
  diagnostics.begin();
}

void loop() {
  diagnostics.update();
}
