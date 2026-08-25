#include "FaultManager.h"

#include <string.h>

void FaultManager::begin(EventBus& events, Logger& logger) {
  events_ = &events;
  logger_ = &logger;
}

void FaultManager::report(FaultCode code, const char* component,
                          const char* message) {
  last_.code = code;
  last_.timestampMs = millis();
  strlcpy(last_.component, component ? component : "system",
          sizeof(last_.component));
  strlcpy(last_.message, message ? message : "unspecified fault",
          sizeof(last_.message));
  ++count_;
  logger_->error(last_.component, "fault %u: %s", static_cast<unsigned>(code),
                 last_.message);
  events_->publish(SystemEventType::FaultReported, static_cast<uint32_t>(code),
                   count_);
}
