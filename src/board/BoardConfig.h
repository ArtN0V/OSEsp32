#pragma once

#include <Arduino.h>

namespace board {

constexpr uint16_t SCREEN_WIDTH = 320;
constexpr uint16_t SCREEN_HEIGHT = 240;

// ILI9341 display (the known-good mapping from CYD_Cheap_Yellow_Display).
constexpr int TFT_SCLK = 14;
constexpr int TFT_MOSI = 13;
constexpr int TFT_MISO = 12;
constexpr int TFT_DC = 2;
constexpr int TFT_CS = 15;
constexpr int TFT_BL = 21;

// XPT2046 resistive touch. It intentionally uses software SPI so that the
// second ESP32 hardware SPI controller remains dedicated to the SD card.
constexpr int TOUCH_IRQ = 36;
constexpr int TOUCH_MOSI = 32;
constexpr int TOUCH_MISO = 39;
constexpr int TOUCH_CLK = 25;
constexpr int TOUCH_CS = 33;

// microSD/TF slot. Stage 0 must validate these pins on the actual board.
constexpr int SD_SCLK = 18;
constexpr int SD_MISO = 19;
constexpr int SD_MOSI = 23;
constexpr int SD_CS = 5;
constexpr uint32_t SD_FREQUENCY = 10000000;

// On-board peripherals found on the common ESP32-2432S028 layout.
constexpr int SPEAKER = 26;
constexpr int LIGHT_SENSOR = 34;
constexpr int LED_RED = 4;
constexpr int LED_GREEN = 16;
constexpr int LED_BLUE = 17;
constexpr bool LED_ACTIVE_LOW = true;

// Initial calibration copied from the working FlappyKiernan project.
// Stage 0 records raw values so these limits can be refined per device.
constexpr int TOUCH_X_MIN = 200;
constexpr int TOUCH_X_MAX = 3700;
constexpr int TOUCH_Y_MIN = 240;
constexpr int TOUCH_Y_MAX = 3800;
constexpr int TOUCH_PRESSURE_MIN = 300;

}  // namespace board
