#pragma once

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <lvgl.h>

#include "../kernel/EventBus.h"
#include "../kernel/Logger.h"

struct StorageEntry {
  char name[49] = {};
  char path[129] = {};
  uint32_t size = 0;
  bool directory = false;
};

class StorageService {
 public:
  static constexpr uint8_t PAGE_ENTRIES = 4;

  StorageService();
  void begin(EventBus& events, Logger& logger);
  void update();
  void registerLvglDriver();
  bool listDirectoryPage(const char* path, uint16_t offset,
                         StorageEntry* entries, uint8_t capacity,
                         uint8_t& count, uint16_t& totalCount);
  bool exists(const char* path) const;
  bool removePath(const char* path);
  bool renamePath(const char* from, const char* to);
  bool replacePathAtomic(const char* completedTemporary,
                         const char* destination);
  bool readFile(const char* path, char* buffer, size_t capacity,
                size_t& length, bool allowTruncate = false);
  bool fileSize(const char* path, uint32_t& size) const;
  bool readFileRange(const char* path, uint32_t offset, uint8_t* buffer,
                     size_t length, size_t& bytesRead) const;
  // Release reusable range streams. A matching path closes only that stream;
  // nullptr closes whichever stream is active.
  void releaseReadFile(const char* path = nullptr) const;
  void releaseWriteFile(const char* path = nullptr) const;
  bool computeFileCrc32(const char* path, uint32_t offset, uint32_t length,
                        uint32_t& crc, uint32_t zeroOffset = UINT32_MAX,
                        uint32_t zeroLength = 0) const;
  bool writeFileAtomic(const char* path, const uint8_t* data, size_t length);
  bool mounted() const { return mounted_; }
  uint32_t generation() const { return generation_; }
  bool makeDirectory(const char* path);
  uint64_t freeBytes() const;
  bool writeRange(const char* path, uint32_t offset, const uint8_t* data,
                   size_t length, bool truncate = false);
  bool recoverBuiltinReplacement(const char* path);
  static bool isImagePath(const char* path);
  static bool isYapPath(const char* path);
  static bool makeLvglPath(const char* sdPath, char* output, size_t outputSize);

 private:
  SPIClass spi_;
  EventBus* events_ = nullptr;
  Logger* logger_ = nullptr;
  bool spiStarted_ = false;
  bool mounted_ = false;
  uint32_t generation_ = 0;
  bool lvglRegistered_ = false;
  bool unavailableReported_ = false;
  uint32_t lastProbeMs_ = 0;
  lv_fs_drv_t lvglDriver_;
  mutable File rangeReadFile_;
  mutable char rangeReadPath_[129] = {};
  mutable File rangeWriteFile_;
  mutable char rangeWritePath_[129] = {};

  bool mount();
  void ensureSystemDirectories();
  void markRemoved();
  static const char* normalizePath(const char* path, char* buffer,
                                   size_t bufferSize);
  static bool readyCallback(lv_fs_drv_t* driver);
  static void* openCallback(lv_fs_drv_t* driver, const char* path,
                            lv_fs_mode_t mode);
  static lv_fs_res_t closeCallback(lv_fs_drv_t* driver, void* file);
  static lv_fs_res_t readCallback(lv_fs_drv_t* driver, void* file, void* buffer,
                                  uint32_t bytesToRead, uint32_t* bytesRead);
  static lv_fs_res_t writeCallback(lv_fs_drv_t* driver, void* file,
                                   const void* buffer, uint32_t bytesToWrite,
                                   uint32_t* bytesWritten);
  static lv_fs_res_t seekCallback(lv_fs_drv_t* driver, void* file,
                                  uint32_t position, lv_fs_whence_t whence);
  static lv_fs_res_t tellCallback(lv_fs_drv_t* driver, void* file,
                                  uint32_t* position);
  static void* directoryOpenCallback(lv_fs_drv_t* driver, const char* path);
  static lv_fs_res_t directoryReadCallback(lv_fs_drv_t* driver, void* directory,
                                           char* name, uint32_t nameLength);
  static lv_fs_res_t directoryCloseCallback(lv_fs_drv_t* driver,
                                            void* directory);
};
