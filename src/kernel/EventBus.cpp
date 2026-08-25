#include "EventBus.h"

bool EventBus::begin() {
  queue_ = xQueueCreateStatic(CAPACITY, sizeof(SystemEvent), queueBuffer_,
                              &queueStorage_);
  return queue_ != nullptr;
}

bool EventBus::publish(SystemEventType type, uint32_t value0, uint32_t value1) {
  if (!queue_) return false;
  SystemEvent event;
  event.type = type;
  event.timestampMs = millis();
  event.value0 = value0;
  event.value1 = value1;
  if (xQueueSend(queue_, &event, 0) == pdTRUE) return true;
  ++droppedCount_;
  return false;
}

bool EventBus::poll(SystemEvent& event) {
  return queue_ && xQueueReceive(queue_, &event, 0) == pdTRUE;
}
