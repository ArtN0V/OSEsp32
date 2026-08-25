#pragma once

#include <Arduino.h>

#include "../board/DisplayDriver.h"
#include "../board/SdCardDriver.h"
#include "../board/TouchDriver.h"

class DiagnosticsApp {
 public:
  void begin();
  void update();

 private:
  enum class Screen { Menu, Touch, Calibration };

  DisplayDriver display_;
  TouchDriver touch_;
  SdCardDriver sd_;
  Screen screen_ = Screen::Menu;
  bool previousTouch_ = false;
  uint32_t lastTouchDraw_ = 0;
  uint8_t calibrationIndex_ = 0;
  uint16_t calibrationSamples_ = 0;
  uint32_t calibrationSumX_ = 0;
  uint32_t calibrationSumY_ = 0;
  bool calibrationWasPressed_ = false;
  bool calibrationIgnoreUntilRelease_ = false;
  TouchPoint calibrationPoints_[5];

  void configurePeripherals();
  void printBanner();
  void printHelp();
  void printSystemInfo();
  void drawMenu();
  void drawStatus(const char* title, const char* line1, const char* line2,
                  uint16_t color);
  void handleSerial(char command);
  void handleMenuTouch(const TouchPoint& point);
  void updateTouchScreen(const TouchPoint& point);
  void startTouchCalibration(bool ignoreCurrentPress);
  void drawCalibrationTarget();
  void updateTouchCalibration(bool pressed, const TouchPoint& point);
  void finishTouchCalibration();
  static bool fitCalibrationAxis(const TouchPoint* points, bool useX,
                                 uint16_t& rawMin, uint16_t& rawMax,
                                 bool& inverted);

  void runAllTests();
  void runDisplayTest();
  void runBacklightTest();
  void runTouchTest();
  void runSdTest();
  void runIoTest();
  void runMemoryTest();
  void runStressTest();
  void setRgb(bool red, bool green, bool blue);
};
