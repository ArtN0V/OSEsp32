#pragma once

#include <Arduino.h>

// Small RTC-backed breadcrumb trail. It survives watchdog/panic/brownout resets
// and lets the on-device System Info page identify where the previous boot was.
enum class ResetCheckpoint : uint8_t {
  None = 0,
  Booting,
  ShellReady,
  CanvasAllocate,
  CanvasFill,
  CanvasAnimate,
  CanvasRelease,
  CanvasReleased,
  CanvasBmpRead,
  CanvasReady,
  CanvasInput,
  CanvasDraw,
  CanvasRedraw,
};

class ResetDiagnostics {
 public:
  static void begin();
  static void mark(ResetCheckpoint checkpoint);
  static int resetReasonCode();
  static const char* resetReasonName();
  static ResetCheckpoint previousCheckpoint();
  static const char* previousCheckpointName();

 private:
  static int resetReason_;
  static ResetCheckpoint previousCheckpoint_;
};
