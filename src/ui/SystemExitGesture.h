#pragma once

#include <stdint.h>

// Invisible recovery gesture using calibrated screen coordinates, not raw ADC.
class SystemExitGesture {
 public:
  void reset() { armed_ = holding_ = false; }
  bool update(bool pressed, int16_t x, int16_t y, uint32_t now) {
    if (!pressed) {
      armed_ = true;
      holding_ = false;
      return false;
    }
    if (!armed_ || x < 0 || y < 0 || x >= 32 || y >= 32) {
      holding_ = false;
      return false;
    }
    if (!holding_) {
      holding_ = true;
      started_ = now;
    }
    if (static_cast<uint32_t>(now - started_) < 2000) return false;
    reset();
    return true;
  }
 private:
  bool armed_ = false;
  bool holding_ = false;
  uint32_t started_ = 0;
};
