#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class DisplayDriver : public lgfx::LGFX_Device {
 public:
  DisplayDriver();
  bool begin();

 private:
  lgfx::Panel_ILI9341 panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;
};
