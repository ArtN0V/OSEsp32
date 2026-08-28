#pragma once

#include <Arduino.h>
#include <time.h>

#include "SystemSettingsService.h"

class DateTimeService {
 public:
  void begin(SystemSettingsService& settings);
  bool valid() const { return valid_; }
  uint64_t utcNow() const;
  int16_t timezoneMinutes() const { return timezoneMinutes_; }
  bool localTime(struct tm& output) const;
  bool setLocal(const struct tm& local, int16_t timezoneMinutes);
  bool setUtc(uint64_t utcSeconds, int16_t timezoneMinutes);
  void formatTime(char* output, size_t capacity) const;
  void formatDate(char* output, size_t capacity) const;
  void formatDateTime(char* output, size_t capacity) const;

 private:
  SystemSettingsService* settings_ = nullptr;
  uint64_t baseUtc_ = 0;
  uint32_t baseMillis_ = 0;
  int16_t timezoneMinutes_ = 180;
  bool valid_ = false;

  static int64_t daysFromCivil(int year, unsigned month, unsigned day);
};
