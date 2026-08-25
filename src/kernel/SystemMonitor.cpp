#include "SystemMonitor.h"

#include <esp_heap_caps.h>

void SystemMonitor::begin(EventBus& events, Logger& logger) {
  events_ = &events;
  logger_ = &logger;
  latest_ = sample();
  lastSampleMs_ = millis();
  logger_->info("memory", "heap=%u min=%u largest=%u psram=%u",
                latest_.freeHeap, latest_.minimumFreeHeap,
                latest_.largestFreeBlock, latest_.freePsram);
}

MemorySnapshot SystemMonitor::sample() const {
  MemorySnapshot snapshot;
  snapshot.freeHeap = ESP.getFreeHeap();
  snapshot.minimumFreeHeap = ESP.getMinFreeHeap();
  snapshot.largestFreeBlock =
      heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  snapshot.freePsram = ESP.getFreePsram();
  return snapshot;
}

void SystemMonitor::update() {
  const uint32_t now = millis();
  if (now - lastSampleMs_ < SAMPLE_INTERVAL_MS) return;
  lastSampleMs_ = now;
  latest_ = sample();
  events_->publish(SystemEventType::MemorySample, latest_.freeHeap,
                   latest_.largestFreeBlock);

  const bool isLow = latest_.freeHeap < LOW_HEAP_BYTES ||
                     latest_.largestFreeBlock < LOW_LARGEST_BLOCK_BYTES;
  if (isLow && !lowMemory_) {
    events_->publish(SystemEventType::LowMemory, latest_.freeHeap,
                     latest_.largestFreeBlock);
    logger_->warning("memory", "low heap=%u largest=%u", latest_.freeHeap,
                     latest_.largestFreeBlock);
  } else if (!isLow && lowMemory_) {
    logger_->info("memory", "memory pressure cleared");
  }
  lowMemory_ = isLow;
}
