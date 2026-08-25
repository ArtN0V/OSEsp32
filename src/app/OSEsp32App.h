#pragma once

#include "../diagnostics/DiagnosticsApp.h"
#include "../kernel/SystemKernel.h"

class OSEsp32App {
 public:
  void begin();
  void update();

 private:
  SystemKernel kernel_;
  DiagnosticsApp diagnostics_;
};
