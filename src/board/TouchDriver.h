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

class TouchDriver {
 public:
  void begin();
  bool read(TouchPoint& point);

 private:
  uint8_t transfer8(uint8_t value);
  uint16_t transfer16(uint16_t value);
  static int16_t bestTwoAverage(int16_t a, int16_t b, int16_t c);
};
