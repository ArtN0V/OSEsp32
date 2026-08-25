#pragma once

#include <Arduino.h>

#include "EventBus.h"
#include "FaultManager.h"
#include "Logger.h"
#include "SystemMonitor.h"

enum class LifecycleState : uint8_t {
  Booting,
  Diagnostics,
  Running,
  SafeMode,
  Fault,
};

class SystemKernel {
 public:
  bool begin();
  void update();
  void setLifecycle(LifecycleState state);

  Logger& logger() { return logger_; }
  EventBus& events() { return events_; }
  FaultManager& faults() { return faults_; }
  SystemMonitor& monitor() { return monitor_; }
  LifecycleState lifecycle() const { return lifecycle_; }
  uint32_t handledEventCount() const { return handledEventCount_; }

 private:
  Logger logger_;
  EventBus events_;
  FaultManager faults_;
  SystemMonitor monitor_;
  LifecycleState lifecycle_ = LifecycleState::Booting;
  uint32_t handledEventCount_ = 0;
  uint32_t reportedDroppedEvents_ = 0;

  void handleEvent(const SystemEvent& event);
  static const char* lifecycleName(LifecycleState state);
};
