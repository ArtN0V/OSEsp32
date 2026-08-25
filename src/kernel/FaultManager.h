#pragma once

#include <Arduino.h>

#include "EventBus.h"
#include "Logger.h"

enum class FaultCode : uint16_t {
  None,
  EventQueueOverflow,
  LowMemory,
  StorageUnavailable,
  CalibrationFailed,
  InternalError,
};

struct FaultRecord {
  FaultCode code = FaultCode::None;
  uint32_t timestampMs = 0;
  char component[13] = {};
  char message[80] = {};
};

class FaultManager {
 public:
  void begin(EventBus& events, Logger& logger);
  void report(FaultCode code, const char* component, const char* message);
  const FaultRecord& last() const { return last_; }
  uint32_t count() const { return count_; }

 private:
  EventBus* events_ = nullptr;
  Logger* logger_ = nullptr;
  FaultRecord last_;
  uint32_t count_ = 0;
};
