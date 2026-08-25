#pragma once

#include "../diagnostics/DiagnosticsApp.h"
#include "../kernel/SystemKernel.h"
#include "../services/BootModeService.h"
#include "../shell/DesktopShell.h"

class OSEsp32App {
 public:
  void begin();
  void update();

 private:
  SystemKernel kernel_;
  BootModeService bootMode_;
  DesktopShell shell_;
  DiagnosticsApp diagnostics_;
  BootMode activeMode_ = BootMode::Shell;
};
