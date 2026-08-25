#pragma once

#include <Arduino.h>

#include "EventBus.h"
#include "Logger.h"

struct MemorySnapshot {
  uint32_t freeHeap = 0;
  uint32_t minimumFreeHeap = 0;
  uint32_t largestFreeBlock = 0;
  uint32_t freePsram = 0;
};

class SystemMonitor {
 public:
  static constexpr uint32_t SAMPLE_INTERVAL_MS = 5000;
  static constexpr uint32_t LOW_HEAP_BYTES = 64 * 1024;
  static constexpr uint32_t LOW_LARGEST_BLOCK_BYTES = 32 * 1024;

  void begin(EventBus& events, Logger& logger);
  void update();
  MemorySnapshot sample() const;
  const MemorySnapshot& latest() const { return latest_; }

 private:
  EventBus* events_ = nullptr;
  Logger* logger_ = nullptr;
  MemorySnapshot latest_;
  uint32_t lastSampleMs_ = 0;
  bool lowMemory_ = false;
};
