#pragma once

#include <Arduino.h>

struct TouchPoint {
  bool pressed = false;
  uint16_t rawX = 0;
  uint16_t rawY = 0;
  uint16_t pressure = 0;
  int16_t x = 0;
  int16_t y = 0;
};

struct TouchCalibration {
  uint16_t rawXMin = 0;
  uint16_t rawXMax = 4095;
  uint16_t rawYMin = 0;
  uint16_t rawYMax = 4095;
  bool invertX = false;
  bool invertY = false;
  bool stored = false;
};

class TouchDriver {
 public:
  void begin();
  bool read(TouchPoint& point);
  bool saveCalibration(uint16_t rawXMin, uint16_t rawXMax,
                       uint16_t rawYMin, uint16_t rawYMax,
                       bool invertX, bool invertY);
  bool resetCalibration();
  const TouchCalibration& calibration() const { return calibration_; }

 private:
  TouchCalibration calibration_;
  uint8_t transfer8(uint8_t value);
  uint16_t transfer16(uint16_t value);
  static int16_t bestTwoAverage(int16_t a, int16_t b, int16_t c);
  void useDefaultCalibration();
  void loadCalibration();
  bool loadCalibrationFrom(const char* nameSpace);
};
