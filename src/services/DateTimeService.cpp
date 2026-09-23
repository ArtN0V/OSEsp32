#include "DateTimeService.h"
#include <esp_timer.h>
#include <limits>

void DateTimeService::begin(SystemSettingsService& settings) {
  settings_ = &settings;
  valid_ = settings.loadClock(baseUtc_, timezoneMinutes_);
  baseMicros_ = esp_timer_get_time();
  if (timezoneMinutes_<-720 || timezoneMinutes_>840 || baseUtc_>static_cast<uint64_t>(std::numeric_limits<time_t>::max())-50400) valid_=false;
}

uint64_t DateTimeService::utcNow() const {
  if (!valid_) return 0;
  return baseUtc_ + static_cast<uint64_t>(esp_timer_get_time() - baseMicros_) / 1000000;
}

bool DateTimeService::localTime(struct tm& output) const {
  if (!valid_) return false;
  const int64_t adjusted=static_cast<int64_t>(utcNow())+
      static_cast<int32_t>(timezoneMinutes_)*60;
  if (adjusted<0 || static_cast<uint64_t>(adjusted)>
      static_cast<uint64_t>(std::numeric_limits<time_t>::max())) return false;
  const time_t localEpoch = static_cast<time_t>(adjusted);
  return gmtime_r(&localEpoch, &output) != nullptr;
}

int64_t DateTimeService::daysFromCivil(int year, unsigned month,
                                       unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned adjustedMonth = static_cast<unsigned>(
      static_cast<int>(month) + (month > 2 ? -3 : 9));
  const unsigned dayOfYear =
      (153 * adjustedMonth + 2) / 5 + day - 1;
  const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 -
                            yearOfEra / 100 + dayOfYear;
  return era * 146097LL + static_cast<int64_t>(dayOfEra) - 719468;
}

bool DateTimeService::setLocal(const struct tm& local,
                               int16_t timezoneMinutes) {
  const int year = local.tm_year + 1900;
  const unsigned month = static_cast<unsigned>(local.tm_mon + 1);
  const unsigned day = static_cast<unsigned>(local.tm_mday);
  static const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  if (year<1970 || year>2099 || month<1 || month>12 || day<1 ||
      local.tm_hour<0 || local.tm_hour>23 || local.tm_min<0 || local.tm_min>59 || local.tm_sec<0 || local.tm_sec>59) return false;
  const bool leap=year%4==0 && (year%100!=0 || year%400==0);
  if (day>days[month-1]+(month==2 && leap ? 1 : 0)) return false;
  const int64_t localSeconds =
      daysFromCivil(year, month, day) * 86400LL + local.tm_hour * 3600LL +
      local.tm_min * 60LL + local.tm_sec;
  const int64_t utcSeconds =
      localSeconds - static_cast<int32_t>(timezoneMinutes) * 60;
  if (utcSeconds <= 0) return false;
  return setUtc(static_cast<uint64_t>(utcSeconds), timezoneMinutes);
}

bool DateTimeService::setUtc(uint64_t utcSeconds, int16_t timezoneMinutes) {
  if (!settings_ || !utcSeconds || timezoneMinutes < -720 ||
      timezoneMinutes > 840 || utcSeconds>static_cast<uint64_t>(std::numeric_limits<time_t>::max())-50400)
    return false;
  if (!settings_->saveClock(utcSeconds, timezoneMinutes)) return false;
  baseUtc_ = utcSeconds;
  baseMicros_ = esp_timer_get_time();
  timezoneMinutes_ = timezoneMinutes;
  valid_ = true;
  return true;
}

void DateTimeService::formatTime(char* output, size_t capacity) const {
  struct tm value;
  if (!output || !capacity) return;
  if (!localTime(value)) {
    strlcpy(output, "--:--", capacity);
    return;
  }
  snprintf(output, capacity, "%02d:%02d", value.tm_hour, value.tm_min);
}

void DateTimeService::formatDate(char* output, size_t capacity) const {
  struct tm value;
  if (!output || !capacity) return;
  if (!localTime(value)) {
    strlcpy(output, "--.--.----", capacity);
    return;
  }
  snprintf(output, capacity, "%02d.%02d.%04d", value.tm_mday,
           value.tm_mon + 1, value.tm_year + 1900);
}

void DateTimeService::formatDateTime(char* output, size_t capacity) const {
  struct tm value;
  if (!output || !capacity) return;
  if (!localTime(value)) {
    strlcpy(output, "Not set", capacity);
    return;
  }
  snprintf(output, capacity, "%02d.%02d.%04d  %02d:%02d", value.tm_mday,
           value.tm_mon + 1, value.tm_year + 1900, value.tm_hour,
           value.tm_min);
}
