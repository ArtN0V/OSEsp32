#pragma once

#include <Arduino.h>

#include "../board/TouchDriver.h"
#include "../kernel/EventBus.h"
#include "../kernel/Logger.h"

enum class CalibrationUpdate : uint8_t {
  None,
  Sampling,
  PointAccepted,
  PointRetry,
  Completed,
  Failed,
};

class TouchCalibrationService {
 public:
  static constexpr uint8_t POINT_COUNT = 5;
  static constexpr uint16_t REQUIRED_SAMPLES = 8;
  static constexpr uint16_t MAX_SAMPLES = 64;

  void begin(TouchDriver& touch, EventBus& events, Logger& logger);
  void start(bool ignoreUntilRelease = false);
  void cancel();
  CalibrationUpdate update(bool pressed, const TouchPoint& point);

  bool active() const { return active_; }
  uint8_t pointIndex() const { return pointIndex_; }
  uint16_t sampleCount() const { return sampleCount_; }
  int16_t targetX() const;
  int16_t targetY() const;
  const TouchCalibration& result() const { return touch_->calibration(); }

 private:
  TouchDriver* touch_ = nullptr;
  EventBus* events_ = nullptr;
  Logger* logger_ = nullptr;
  bool active_ = false;
  bool ignoreUntilRelease_ = false;
  bool wasPressed_ = false;
  uint8_t pointIndex_ = 0;
  uint16_t sampleCount_ = 0;
  uint32_t sumX_ = 0;
  uint32_t sumY_ = 0;
  TouchPoint points_[POINT_COUNT];

  void resetSample();
  bool finish();
  static bool fitAxis(const TouchPoint* points, bool useX, uint16_t& rawMin,
                      uint16_t& rawMax, bool& inverted);
};
