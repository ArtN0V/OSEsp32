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
  enum class Screen { Menu, Touch };

  DisplayDriver display_;
  TouchDriver touch_;
  SdCardDriver sd_;
  Screen screen_ = Screen::Menu;
  bool previousTouch_ = false;
  uint32_t lastTouchDraw_ = 0;

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
