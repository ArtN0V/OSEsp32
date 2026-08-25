#include "SystemKernel.h"

#include <esp_system.h>

bool SystemKernel::begin() {
  const bool loggerOk = logger_.begin();
  const bool eventsOk = events_.begin();
  if (!loggerOk || !eventsOk) return false;

  faults_.begin(events_, logger_);
  monitor_.begin(events_, logger_);
  logger_.info("kernel", "OSEsp32 kernel starting; reset reason=%d",
               static_cast<int>(esp_reset_reason()));
  events_.publish(SystemEventType::BootCompleted, ESP.getFreeHeap(),
                  ESP.getFlashChipSize());
  setLifecycle(LifecycleState::Diagnostics);
  return true;
}

const char* SystemKernel::lifecycleName(LifecycleState state) {
  switch (state) {
    case LifecycleState::Booting: return "booting";
    case LifecycleState::Diagnostics: return "diagnostics";
    case LifecycleState::Running: return "running";
    case LifecycleState::SafeMode: return "safe-mode";
    case LifecycleState::Fault: return "fault";
  }
  return "unknown";
}

void SystemKernel::setLifecycle(LifecycleState state) {
  if (state == lifecycle_) return;
  lifecycle_ = state;
  logger_.info("lifecycle", "state=%s", lifecycleName(state));
  events_.publish(SystemEventType::LifecycleChanged,
                  static_cast<uint32_t>(state));
}

void SystemKernel::handleEvent(const SystemEvent& event) {
  ++handledEventCount_;
  switch (event.type) {
    case SystemEventType::LowMemory:
      if (faults_.last().code != FaultCode::LowMemory) {
        faults_.report(FaultCode::LowMemory, "memory", "safety threshold crossed");
      }
      break;
    case SystemEventType::TouchCalibrationCompleted:
      logger_.info("touch", "calibration stored in NVS");
      break;
    case SystemEventType::TouchCalibrationFailed:
      faults_.report(FaultCode::CalibrationFailed, "touch",
                     "five-point fit rejected");
      break;
    default:
      break;
  }
}

void SystemKernel::update() {
  monitor_.update();
  SystemEvent event;
  while (events_.poll(event)) handleEvent(event);

  const uint32_t dropped = events_.droppedCount();
  if (dropped != reportedDroppedEvents_) {
    reportedDroppedEvents_ = dropped;
    faults_.report(FaultCode::EventQueueOverflow, "events",
                   "one or more system events were dropped");
  }
}
