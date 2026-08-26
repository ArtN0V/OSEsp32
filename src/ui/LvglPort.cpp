#include "LvglPort.h"

bool LvglPort::begin(bool rotation180, const lv_font_t* interfaceFont) {
  rotation180_ = rotation180;
  if (!displayDriver_.begin()) return false;
  if (rotation180_) displayDriver_.setRotation(3);
  displayDriver_.initDMA();
  touchDriver_.begin();

  lv_init();
  lv_tick_set_cb(tickCallback);

  lvDisplay_ = lv_display_create(board::SCREEN_WIDTH, board::SCREEN_HEIGHT);
  if (!lvDisplay_) return false;
  lv_display_set_driver_data(lvDisplay_, this);
  lv_display_set_color_format(lvDisplay_, LV_COLOR_FORMAT_RGB565_SWAPPED);
  lv_display_set_flush_cb(lvDisplay_, flushCallback);
  lv_display_set_buffers(lvDisplay_, drawBuffer1_, drawBuffer2_,
                         sizeof(drawBuffer1_), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_default(lvDisplay_);

  pointer_ = lv_indev_create();
  if (!pointer_) return false;
  lv_indev_set_type(pointer_, LV_INDEV_TYPE_POINTER);
  lv_indev_set_driver_data(pointer_, this);
  lv_indev_set_read_cb(pointer_, pointerReadCallback);

  lv_theme_t* theme = lv_theme_default_init(
      lvDisplay_, lv_color_hex(0x0078D4), lv_color_hex(0x38A169), false,
      interfaceFont ? interfaceFont : &lv_font_montserrat_14);
  lv_display_set_theme(lvDisplay_, theme);
  return true;
}

void LvglPort::update() {
  touchPressed_ = touchDriver_.read(touchPoint_);
  if (touchPressed_ && rotation180_) {
    touchPoint_.x = board::SCREEN_WIDTH - 1 - touchPoint_.x;
    touchPoint_.y = board::SCREEN_HEIGHT - 1 - touchPoint_.y;
  }
  lv_timer_handler();
}

uint32_t LvglPort::tickCallback() { return millis(); }

void LvglPort::flushCallback(lv_display_t* display, const lv_area_t* area,
                             uint8_t* pixels) {
  LvglPort* port =
      static_cast<LvglPort*>(lv_display_get_driver_data(display));
  const int32_t width = area->x2 - area->x1 + 1;
  const int32_t height = area->y2 - area->y1 + 1;
  port->displayDriver_.startWrite();
  port->displayDriver_.pushImageDMA(area->x1, area->y1, width, height,
                                    reinterpret_cast<uint16_t*>(pixels));
  port->displayDriver_.waitDMA();
  port->displayDriver_.endWrite();
  lv_display_flush_ready(display);
}

void LvglPort::pointerReadCallback(lv_indev_t* indev,
                                   lv_indev_data_t* data) {
  LvglPort* port = static_cast<LvglPort*>(lv_indev_get_driver_data(indev));
  if (port->pointerSuspended_ || !port->touchPressed_) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  data->state = LV_INDEV_STATE_PRESSED;
  data->point.x = port->touchPoint_.x;
  data->point.y = port->touchPoint_.y;
}
