#include "TouchCalibrationService.h"

#include <math.h>

#include "../board/BoardConfig.h"

namespace {
constexpr int16_t TARGET_X[TouchCalibrationService::POINT_COUNT] = {
    20, 299, 299, 20, 160};
constexpr int16_t TARGET_Y[TouchCalibrationService::POINT_COUNT] = {
    20, 20, 219, 219, 120};
}

void TouchCalibrationService::begin(TouchDriver& touch, EventBus& events,
                                    Logger& logger) {
  touch_ = &touch;
  events_ = &events;
  logger_ = &logger;
}

void TouchCalibrationService::start(bool ignoreUntilRelease) {
  active_ = true;
  ignoreUntilRelease_ = ignoreUntilRelease;
  wasPressed_ = false;
  pointIndex_ = 0;
  resetSample();
  logger_->info("touch-cal", "five-point calibration started");
  events_->publish(SystemEventType::TouchCalibrationStarted);
}

int16_t TouchCalibrationService::targetX() const {
  const uint8_t index = pointIndex_ < POINT_COUNT ? pointIndex_ : POINT_COUNT - 1;
  return TARGET_X[index];
}

int16_t TouchCalibrationService::targetY() const {
  const uint8_t index = pointIndex_ < POINT_COUNT ? pointIndex_ : POINT_COUNT - 1;
  return TARGET_Y[index];
}

void TouchCalibrationService::resetSample() {
  sampleCount_ = 0;
  sumX_ = 0;
  sumY_ = 0;
}

CalibrationUpdate TouchCalibrationService::update(bool pressed,
                                                   const TouchPoint& point) {
  if (!active_) return CalibrationUpdate::None;
  if (ignoreUntilRelease_) {
    if (!pressed) ignoreUntilRelease_ = false;
    return CalibrationUpdate::None;
  }

  if (pressed) {
    if (!wasPressed_) {
      wasPressed_ = true;
      resetSample();
    }
    if (sampleCount_ < MAX_SAMPLES) {
      sumX_ += point.rawX;
      sumY_ += point.rawY;
      ++sampleCount_;
    }
    return CalibrationUpdate::Sampling;
  }

  if (!wasPressed_) return CalibrationUpdate::None;
  wasPressed_ = false;
  if (sampleCount_ < REQUIRED_SAMPLES) {
    logger_->warning("touch-cal", "point %u press too short", pointIndex_ + 1);
    resetSample();
    return CalibrationUpdate::PointRetry;
  }

  points_[pointIndex_].rawX = sumX_ / sampleCount_;
  points_[pointIndex_].rawY = sumY_ / sampleCount_;
  logger_->info("touch-cal", "point %u raw=%u,%u samples=%u", pointIndex_ + 1,
                points_[pointIndex_].rawX, points_[pointIndex_].rawY,
                sampleCount_);
  events_->publish(SystemEventType::TouchCalibrationPoint, pointIndex_,
                   sampleCount_);
  ++pointIndex_;
  resetSample();

  if (pointIndex_ < POINT_COUNT) return CalibrationUpdate::PointAccepted;
  active_ = false;
  if (finish()) {
    events_->publish(SystemEventType::TouchCalibrationCompleted);
    return CalibrationUpdate::Completed;
  }
  events_->publish(SystemEventType::TouchCalibrationFailed);
  return CalibrationUpdate::Failed;
}

bool TouchCalibrationService::fitAxis(const TouchPoint* points, bool useX,
                                      uint16_t& rawMin, uint16_t& rawMax,
                                      bool& inverted) {
  double sumScreen = 0;
  double sumRaw = 0;
  double sumScreenSquared = 0;
  double sumScreenRaw = 0;
  const double screenMaximum = useX ? board::SCREEN_WIDTH - 1
                                    : board::SCREEN_HEIGHT - 1;

  for (uint8_t index = 0; index < POINT_COUNT; ++index) {
    const double screen = useX ? TARGET_X[index] : TARGET_Y[index];
    const double raw = useX ? points[index].rawX : points[index].rawY;
    sumScreen += screen;
    sumRaw += raw;
    sumScreenSquared += screen * screen;
    sumScreenRaw += screen * raw;
  }

  const double denominator = POINT_COUNT * sumScreenSquared -
                             sumScreen * sumScreen;
  if (fabs(denominator) < 1.0) return false;
  const double slope = (POINT_COUNT * sumScreenRaw - sumScreen * sumRaw) /
                       denominator;
  const double intercept = (sumRaw - slope * sumScreen) / POINT_COUNT;
  const int rawAtStart = constrain(static_cast<int>(lround(intercept)), 0, 4095);
  const int rawAtEnd = constrain(
      static_cast<int>(lround(intercept + slope * screenMaximum)), 0, 4095);
  inverted = rawAtStart > rawAtEnd;
  rawMin = min(rawAtStart, rawAtEnd);
  rawMax = max(rawAtStart, rawAtEnd);
  return rawMax > rawMin + 500;
}

bool TouchCalibrationService::finish() {
  uint16_t xMin = 0;
  uint16_t xMax = 0;
  uint16_t yMin = 0;
  uint16_t yMax = 0;
  bool invertX = false;
  bool invertY = false;
  const bool valid = fitAxis(points_, true, xMin, xMax, invertX) &&
                     fitAxis(points_, false, yMin, yMax, invertY);
  if (!valid ||
      !touch_->saveCalibration(xMin, xMax, yMin, yMax, invertX, invertY)) {
    logger_->error("touch-cal", "calibration fit rejected");
    return false;
  }
  logger_->info("touch-cal", "saved X=%u..%u inv=%u Y=%u..%u inv=%u", xMin,
                xMax, invertX, yMin, yMax, invertY);
  return true;
}
