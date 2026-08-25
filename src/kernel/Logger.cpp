#include "Logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

bool Logger::begin() {
  mutex_ = xSemaphoreCreateMutexStatic(&mutexStorage_);
  return mutex_ != nullptr;
}

char Logger::levelLetter(LogLevel level) {
  switch (level) {
    case LogLevel::Debug: return 'D';
    case LogLevel::Info: return 'I';
    case LogLevel::Warning: return 'W';
    case LogLevel::Error: return 'E';
  }
  return '?';
}

void Logger::writeFormatted(LogLevel level, const char* tag, const char* format,
                            va_list arguments) {
  char message[96];
  vsnprintf(message, sizeof(message), format, arguments);
  const uint32_t timestamp = millis();

  if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
  LogEntry& entry = entries_[writeIndex_];
  entry.timestampMs = timestamp;
  entry.level = level;
  strlcpy(entry.tag, tag ? tag : "system", sizeof(entry.tag));
  strlcpy(entry.message, message, sizeof(entry.message));
  writeIndex_ = (writeIndex_ + 1) % CAPACITY;
  if (count_ < CAPACITY) ++count_;
  if (mutex_) xSemaphoreGive(mutex_);

  Serial.printf("[%10lu] %c/%s: %s\n", static_cast<unsigned long>(timestamp),
                levelLetter(level), tag ? tag : "system", message);
}

void Logger::log(LogLevel level, const char* tag, const char* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  writeFormatted(level, tag, format, arguments);
  va_end(arguments);
}

#define LOGGER_LEVEL_METHOD(name, level)                                      \
  void Logger::name(const char* tag, const char* format, ...) {                \
    va_list arguments;                                                         \
    va_start(arguments, format);                                                \
    writeFormatted(level, tag, format, arguments);                              \
    va_end(arguments);                                                          \
  }

LOGGER_LEVEL_METHOD(debug, LogLevel::Debug)
LOGGER_LEVEL_METHOD(info, LogLevel::Info)
LOGGER_LEVEL_METHOD(warning, LogLevel::Warning)
LOGGER_LEVEL_METHOD(error, LogLevel::Error)

bool Logger::copyOldest(uint8_t index, LogEntry& entry) {
  if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
  if (index >= count_) {
    if (mutex_) xSemaphoreGive(mutex_);
    return false;
  }
  const uint8_t oldest = (writeIndex_ + CAPACITY - count_) % CAPACITY;
  entry = entries_[(oldest + index) % CAPACITY];
  if (mutex_) xSemaphoreGive(mutex_);
  return true;
}
