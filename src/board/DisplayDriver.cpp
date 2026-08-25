#include "DisplayDriver.h"

#include "BoardConfig.h"

DisplayDriver::DisplayDriver() {
  {
    auto cfg = bus_.config();
    cfg.spi_host = HSPI_HOST;
    cfg.spi_mode = 0;
    cfg.freq_write = 27000000;
    cfg.freq_read = 1600000;
    cfg.spi_3wire = false;
    cfg.use_lock = true;
    cfg.dma_channel = 1;
    cfg.pin_sclk = board::TFT_SCLK;
    cfg.pin_mosi = board::TFT_MOSI;
    cfg.pin_miso = board::TFT_MISO;
    cfg.pin_dc = board::TFT_DC;
    bus_.config(cfg);
    panel_.setBus(&bus_);
  }

  {
    auto cfg = panel_.config();
    cfg.pin_cs = board::TFT_CS;
    cfg.pin_rst = -1;
    cfg.pin_busy = -1;
    cfg.memory_width = 240;
    cfg.memory_height = 320;
    cfg.panel_width = 240;
    cfg.panel_height = 320;
    cfg.offset_x = 0;
    cfg.offset_y = 0;
    cfg.offset_rotation = 0;
    cfg.dummy_read_pixel = 8;
    cfg.dummy_read_bits = 1;
    cfg.readable = true;
    cfg.invert = false;
    cfg.rgb_order = false;
    cfg.dlen_16bit = false;
    cfg.bus_shared = true;
    panel_.config(cfg);
  }

  {
    auto cfg = light_.config();
    cfg.pin_bl = board::TFT_BL;
    cfg.invert = false;
    cfg.freq = 44100;
    cfg.pwm_channel = 7;
    light_.config(cfg);
    panel_.setLight(&light_);
  }

  setPanel(&panel_);
}

bool DisplayDriver::begin() {
  init();
  setRotation(1);
  setBrightness(255);
  return width() == board::SCREEN_WIDTH && height() == board::SCREEN_HEIGHT;
}
