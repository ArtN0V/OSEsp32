#include "TouchDriver.h"

#include <Preferences.h>

#include "BoardConfig.h"

namespace {
constexpr const char* NVS_NAMESPACE = "osesp32_touch";
constexpr const char* LEGACY_NVS_NAMESPACE = "yellow_touch";
constexpr uint8_t CALIBRATION_VERSION = 1;
}

void TouchDriver::begin() {
  pinMode(board::TOUCH_CS, OUTPUT);
  pinMode(board::TOUCH_CLK, OUTPUT);
  pinMode(board::TOUCH_MOSI, OUTPUT);
  pinMode(board::TOUCH_MISO, INPUT);
  pinMode(board::TOUCH_IRQ, INPUT);
  digitalWrite(board::TOUCH_CS, HIGH);
  digitalWrite(board::TOUCH_CLK, LOW);
  useDefaultCalibration();
  loadCalibration();
}

void TouchDriver::useDefaultCalibration() {
  calibration_.rawXMin = board::TOUCH_X_MIN;
  calibration_.rawXMax = board::TOUCH_X_MAX;
  calibration_.rawYMin = board::TOUCH_Y_MIN;
  calibration_.rawYMax = board::TOUCH_Y_MAX;
  calibration_.invertX = board::TOUCH_INVERT_X;
  calibration_.invertY = board::TOUCH_INVERT_Y;
  calibration_.stored = false;
}

void TouchDriver::loadCalibration() {
  if (loadCalibrationFrom(NVS_NAMESPACE)) return;
  if (loadCalibrationFrom(LEGACY_NVS_NAMESPACE)) {
    // Preserve calibration made by early development builds while migrating
    // the persisted copy to the final OSEsp32 namespace.
    saveCalibration(calibration_.rawXMin, calibration_.rawXMax,
                    calibration_.rawYMin, calibration_.rawYMax,
                    calibration_.invertX, calibration_.invertY);
  }
}

bool TouchDriver::loadCalibrationFrom(const char* nameSpace) {
  Preferences preferences;
  if (!preferences.begin(nameSpace, true)) return false;
  bool loaded = false;
  if (preferences.getUChar("version", 0) == CALIBRATION_VERSION) {
    const uint16_t xMin = preferences.getUShort("xmin", calibration_.rawXMin);
    const uint16_t xMax = preferences.getUShort("xmax", calibration_.rawXMax);
    const uint16_t yMin = preferences.getUShort("ymin", calibration_.rawYMin);
    const uint16_t yMax = preferences.getUShort("ymax", calibration_.rawYMax);
    if (xMax > xMin + 500 && yMax > yMin + 500) {
      calibration_.rawXMin = xMin;
      calibration_.rawXMax = xMax;
      calibration_.rawYMin = yMin;
      calibration_.rawYMax = yMax;
      calibration_.invertX = preferences.getBool("invx", calibration_.invertX);
      calibration_.invertY = preferences.getBool("invy", calibration_.invertY);
      calibration_.stored = true;
      loaded = true;
    }
  }
  preferences.end();
  return loaded;
}

bool TouchDriver::saveCalibration(uint16_t rawXMin, uint16_t rawXMax,
                                  uint16_t rawYMin, uint16_t rawYMax,
                                  bool invertX, bool invertY) {
  if (rawXMax <= rawXMin + 500 || rawYMax <= rawYMin + 500 ||
      rawXMax > 4095 || rawYMax > 4095) {
    return false;
  }

  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;
  bool ok = true;
  ok &= preferences.putUShort("xmin", rawXMin) == sizeof(uint16_t);
  ok &= preferences.putUShort("xmax", rawXMax) == sizeof(uint16_t);
  ok &= preferences.putUShort("ymin", rawYMin) == sizeof(uint16_t);
  ok &= preferences.putUShort("ymax", rawYMax) == sizeof(uint16_t);
  ok &= preferences.putBool("invx", invertX) == sizeof(bool);
  ok &= preferences.putBool("invy", invertY) == sizeof(bool);
  ok &= preferences.putUChar("version", CALIBRATION_VERSION) == sizeof(uint8_t);
  preferences.end();
  if (!ok) return false;

  calibration_.rawXMin = rawXMin;
  calibration_.rawXMax = rawXMax;
  calibration_.rawYMin = rawYMin;
  calibration_.rawYMax = rawYMax;
  calibration_.invertX = invertX;
  calibration_.invertY = invertY;
  calibration_.stored = true;
  return true;
}

bool TouchDriver::resetCalibration() {
  Preferences preferences;
  if (!preferences.begin(NVS_NAMESPACE, false)) return false;
  bool ok = preferences.clear();
  preferences.end();
  if (preferences.begin(LEGACY_NVS_NAMESPACE, false)) {
    ok &= preferences.clear();
    preferences.end();
  }
  useDefaultCalibration();
  return ok;
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
      map(point.rawX, calibration_.rawXMin, calibration_.rawXMax,
          0, board::SCREEN_WIDTH - 1),
      0, board::SCREEN_WIDTH - 1);
  const int16_t mappedY = constrain(
      map(point.rawY, calibration_.rawYMin, calibration_.rawYMax,
          0, board::SCREEN_HEIGHT - 1),
      0, board::SCREEN_HEIGHT - 1);
  point.x = calibration_.invertX ? board::SCREEN_WIDTH - 1 - mappedX : mappedX;
  point.y = calibration_.invertY ? board::SCREEN_HEIGHT - 1 - mappedY : mappedY;
  point.pressed = true;
  return true;
}
