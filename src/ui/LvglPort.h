#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "../board/BoardConfig.h"
#include "../board/DisplayDriver.h"
#include "../board/TouchDriver.h"

class LvglPort {
 public:
  static constexpr uint16_t DRAW_LINES = 20;

  bool begin(bool rotation180 = false);
  void update();
  void suspendPointer(bool suspended) { pointerSuspended_ = suspended; }

  DisplayDriver& displayDriver() { return displayDriver_; }
  TouchDriver& touchDriver() { return touchDriver_; }
  bool touchPressed() const { return touchPressed_; }
  const TouchPoint& touchPoint() const { return touchPoint_; }

 private:
  DisplayDriver displayDriver_;
  TouchDriver touchDriver_;
  lv_display_t* lvDisplay_ = nullptr;
  lv_indev_t* pointer_ = nullptr;
  alignas(4) lv_color_t drawBuffer1_[board::SCREEN_WIDTH * DRAW_LINES];
  alignas(4) lv_color_t drawBuffer2_[board::SCREEN_WIDTH * DRAW_LINES];
  TouchPoint touchPoint_;
  bool touchPressed_ = false;
  bool pointerSuspended_ = false;
  bool rotation180_ = false;

  static uint32_t tickCallback();
  static void flushCallback(lv_display_t* display, const lv_area_t* area,
                            uint8_t* pixels);
  static void pointerReadCallback(lv_indev_t* indev, lv_indev_data_t* data);
};
