#include "ResetDiagnostics.h"

#include <esp_attr.h>
#include <esp_system.h>

namespace {
constexpr uint32_t RETAINED_MAGIC = 0x4F534552;  // "OSER"

struct RetainedResetState {
  uint32_t magic;
  uint8_t checkpoint;
  uint8_t reserved[3];
};

RTC_DATA_ATTR RetainedResetState retainedResetState;
}  // namespace

int ResetDiagnostics::resetReason_ = static_cast<int>(ESP_RST_UNKNOWN);
ResetCheckpoint ResetDiagnostics::previousCheckpoint_ = ResetCheckpoint::None;

void ResetDiagnostics::begin() {
  resetReason_ = static_cast<int>(esp_reset_reason());
  if (retainedResetState.magic == RETAINED_MAGIC &&
      retainedResetState.checkpoint <=
          static_cast<uint8_t>(ResetCheckpoint::CanvasRedraw)) {
    previousCheckpoint_ =
        static_cast<ResetCheckpoint>(retainedResetState.checkpoint);
  } else {
    previousCheckpoint_ = ResetCheckpoint::None;
  }
  retainedResetState.magic = RETAINED_MAGIC;
  retainedResetState.checkpoint = static_cast<uint8_t>(ResetCheckpoint::Booting);
}

void ResetDiagnostics::mark(ResetCheckpoint checkpoint) {
  retainedResetState.magic = RETAINED_MAGIC;
  retainedResetState.checkpoint = static_cast<uint8_t>(checkpoint);
}

int ResetDiagnostics::resetReasonCode() { return resetReason_; }

const char* ResetDiagnostics::resetReasonName() {
  switch (static_cast<esp_reset_reason_t>(resetReason_)) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt-wdt";
    case ESP_RST_TASK_WDT: return "task-wdt";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep-sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    default: return "unknown";
  }
}

ResetCheckpoint ResetDiagnostics::previousCheckpoint() {
  return previousCheckpoint_;
}

const char* ResetDiagnostics::previousCheckpointName() {
  switch (previousCheckpoint_) {
    case ResetCheckpoint::None: return "none";
    case ResetCheckpoint::Booting: return "booting";
    case ResetCheckpoint::ShellReady: return "shell-ready";
    case ResetCheckpoint::CanvasAllocate: return "canvas-allocate";
    case ResetCheckpoint::CanvasFill: return "canvas-fill";
    case ResetCheckpoint::CanvasAnimate: return "canvas-animate";
    case ResetCheckpoint::CanvasRelease: return "canvas-release";
    case ResetCheckpoint::CanvasReleased: return "canvas-released";
    case ResetCheckpoint::CanvasBmpRead: return "canvas-bmp-read";
    case ResetCheckpoint::CanvasReady: return "canvas-ready";
    case ResetCheckpoint::CanvasInput: return "canvas-input";
    case ResetCheckpoint::CanvasDraw: return "canvas-draw";
    case ResetCheckpoint::CanvasRedraw: return "canvas-redraw";
  }
  return "invalid";
}
