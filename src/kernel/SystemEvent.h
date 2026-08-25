#pragma once

#include <Arduino.h>

enum class SystemEventType : uint8_t {
  BootCompleted,
  LifecycleChanged,
  MemorySample,
  LowMemory,
  FaultReported,
  TouchCalibrationStarted,
  TouchCalibrationPoint,
  TouchCalibrationCompleted,
  TouchCalibrationFailed,
  TouchCalibrationCancelled,
  ShellReady,
  ShellApplicationOpened,
};

struct SystemEvent {
  SystemEventType type = SystemEventType::BootCompleted;
  uint32_t timestampMs = 0;
  uint32_t value0 = 0;
  uint32_t value1 = 0;
};
