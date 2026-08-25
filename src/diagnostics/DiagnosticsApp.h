#pragma once

#include <Arduino.h>

#include "../board/DisplayDriver.h"
#include "../board/SdCardDriver.h"
#include "../board/TouchDriver.h"
#include "../kernel/SystemKernel.h"
#include "../services/TouchCalibrationService.h"

class DiagnosticsApp {
 public:
  void begin(SystemKernel& kernel);
  void update();

 private:
  enum class Screen { Menu, Touch, Calibration };

  DisplayDriver display_;
  TouchDriver touch_;
  SdCardDriver sd_;
  SystemKernel* kernel_ = nullptr;
  TouchCalibrationService calibration_;
  Screen screen_ = Screen::Menu;
  bool previousTouch_ = false;
  uint32_t lastTouchDraw_ = 0;

  void configurePeripherals();
  void printBanner();
  void printHelp();
  void printSystemInfo();
  void printKernelInfo();
  void drawMenu();
  void drawStatus(const char* title, const char* line1, const char* line2,
                  uint16_t color);
  void handleSerial(char command);
  void handleMenuTouch(const TouchPoint& point);
  void returnToShell();
  void updateTouchScreen(const TouchPoint& point);
  void startTouchCalibration(bool ignoreCurrentPress);
  void drawCalibrationTarget();
  void updateTouchCalibration(bool pressed, const TouchPoint& point);

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
