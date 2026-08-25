#include "TouchDriver.h"

#include "BoardConfig.h"

void TouchDriver::begin() {
  pinMode(board::TOUCH_CS, OUTPUT);
  pinMode(board::TOUCH_CLK, OUTPUT);
  pinMode(board::TOUCH_MOSI, OUTPUT);
  pinMode(board::TOUCH_MISO, INPUT);
  pinMode(board::TOUCH_IRQ, INPUT);
  digitalWrite(board::TOUCH_CS, HIGH);
  digitalWrite(board::TOUCH_CLK, LOW);
}

uint8_t TouchDriver::transfer8(uint8_t value) {
  uint8_t result = 0;
  for (uint8_t mask = 0x80; mask; mask >>= 1) {
    digitalWrite(board::TOUCH_MOSI, value & mask ? HIGH : LOW);
    digitalWrite(board::TOUCH_CLK, HIGH);
    result = static_cast<uint8_t>((result << 1) | digitalRead(board::TOUCH_MISO));
    digitalWrite(board::TOUCH_CLK, LOW);
  }
  return result;
}

uint16_t TouchDriver::transfer16(uint16_t value) {
  return static_cast<uint16_t>(transfer8(value >> 8) << 8) | transfer8(value & 0xFF);
}

int16_t TouchDriver::bestTwoAverage(int16_t a, int16_t b, int16_t c) {
  const int16_t ab = abs(a - b);
  const int16_t ac = abs(a - c);
  const int16_t bc = abs(b - c);
  if (ab <= ac && ab <= bc) return (a + b) >> 1;
  if (ac <= ab && ac <= bc) return (a + c) >> 1;
  return (b + c) >> 1;
}

bool TouchDriver::read(TouchPoint& point) {
  point.pressed = false;
  if (digitalRead(board::TOUCH_IRQ) != LOW) return false;

  int16_t data[6] = {};
  digitalWrite(board::TOUCH_CS, LOW);

  transfer8(0xB1);  // Start Z1 conversion.
  const int16_t z1 = transfer16(0xC1) >> 3;
  int32_t pressure = z1 + 4095;
  const int16_t z2 = transfer16(0x91) >> 3;
  pressure -= z2;

  if (pressure >= board::TOUCH_PRESSURE_MIN) {
    transfer16(0x91);  // First coordinate conversion is commonly noisy.
    data[0] = transfer16(0xD1) >> 3;
    data[1] = transfer16(0x91) >> 3;
    data[2] = transfer16(0xD1) >> 3;
    data[3] = transfer16(0x91) >> 3;
  }
  data[4] = transfer16(0xD0) >> 3;  // Final conversion powers down IRQ.
  data[5] = transfer16(0x0000) >> 3;
  digitalWrite(board::TOUCH_CS, HIGH);

  if (pressure < board::TOUCH_PRESSURE_MIN) return false;

  // This is XPT2046 rotation 1, matching the working reference project.
  point.rawX = bestTwoAverage(data[0], data[2], data[4]);
  point.rawY = bestTwoAverage(data[1], data[3], data[5]);
  point.pressure = static_cast<uint16_t>(constrain(pressure, 0, 65535));
  const int16_t mappedX = constrain(
      map(point.rawX, board::TOUCH_X_MIN, board::TOUCH_X_MAX,
          0, board::SCREEN_WIDTH - 1),
      0, board::SCREEN_WIDTH - 1);
  const int16_t mappedY = constrain(
      map(point.rawY, board::TOUCH_Y_MIN, board::TOUCH_Y_MAX,
          0, board::SCREEN_HEIGHT - 1),
      0, board::SCREEN_HEIGHT - 1);
  point.x = board::TOUCH_INVERT_X ? board::SCREEN_WIDTH - 1 - mappedX : mappedX;
  point.y = board::TOUCH_INVERT_Y ? board::SCREEN_HEIGHT - 1 - mappedY : mappedY;
  point.pressed = true;
  return true;
}
