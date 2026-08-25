#pragma once

#include <Arduino.h>
#include <stdarg.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

enum class LogLevel : uint8_t { Debug, Info, Warning, Error };

struct LogEntry {
  uint32_t timestampMs = 0;
  LogLevel level = LogLevel::Info;
  char tag[13] = {};
  char message[96] = {};
};

class Logger {
 public:
  static constexpr uint8_t CAPACITY = 24;

  bool begin();
  void log(LogLevel level, const char* tag, const char* format, ...)
      __attribute__((format(printf, 4, 5)));
  void debug(const char* tag, const char* format, ...)
      __attribute__((format(printf, 3, 4)));
  void info(const char* tag, const char* format, ...)
      __attribute__((format(printf, 3, 4)));
  void warning(const char* tag, const char* format, ...)
      __attribute__((format(printf, 3, 4)));
  void error(const char* tag, const char* format, ...)
      __attribute__((format(printf, 3, 4)));
  uint8_t count() const { return count_; }
  bool copyOldest(uint8_t index, LogEntry& entry);

 private:
  LogEntry entries_[CAPACITY];
  uint8_t writeIndex_ = 0;
  uint8_t count_ = 0;
  StaticSemaphore_t mutexStorage_;
  SemaphoreHandle_t mutex_ = nullptr;

  void writeFormatted(LogLevel level, const char* tag, const char* format,
                      va_list arguments);
  static char levelLetter(LogLevel level);
};
