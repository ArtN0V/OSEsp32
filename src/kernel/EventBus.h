#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "SystemEvent.h"

class EventBus {
 public:
  static constexpr uint8_t CAPACITY = 16;

  bool begin();
  bool publish(SystemEventType type, uint32_t value0 = 0, uint32_t value1 = 0);
  bool poll(SystemEvent& event);
  uint32_t droppedCount() const { return droppedCount_; }

 private:
  StaticQueue_t queueStorage_;
  uint8_t queueBuffer_[CAPACITY * sizeof(SystemEvent)];
  QueueHandle_t queue_ = nullptr;
  volatile uint32_t droppedCount_ = 0;
};
