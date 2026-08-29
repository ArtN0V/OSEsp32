#pragma once

#include <Arduino.h>

#include "StorageService.h"
#include "../kernel/Logger.h"

enum class YapError : uint8_t {
  None = 0,
  StorageUnavailable,
  IoError,
  TooSmall,
  TooLarge,
  BadMagic,
  UnsupportedVersion,
  BadHeader,
  BadPackageCrc,
  BadSectionTable,
  BadSectionBounds,
  OverlappingSections,
  DuplicateSection,
  BadSectionCrc,
  BadManifest,
  UnsupportedApi,
  UnsupportedCapability,
};

enum class YapLaunchMode : uint8_t {
  Windowed = 0,
  Fullscreen = 1,
  Exclusive = 2,
};

enum YapCapability : uint32_t {
  YapPrivateRead = 1u << 0,
  YapPrivateWrite = 1u << 1,
  YapDocumentsOpen = 1u << 2,
  YapDocumentsCreate = 1u << 3,
  YapDocumentsReplace = 1u << 4,
};

struct YapSection {
  uint32_t type = 0;
  uint32_t offset = 0;
  uint32_t length = 0;
  uint32_t crc32 = 0;
  uint32_t flags = 0;
};

struct YapManifest {
  static constexpr uint8_t MAX_ASSOCIATIONS = 4;

  uint8_t apiMajor = 0;
  uint8_t apiMinor = 0;
  YapLaunchMode launchMode = YapLaunchMode::Windowed;
  uint32_t requestedMemory = 0;
  uint32_t capabilities = 0;
  uint16_t codeSection = 0;
  uint16_t iconSection = UINT16_MAX;
  char appId[33] = {};
  char name[49] = {};
  char entryPoint[25] = {};
  uint8_t associationCount = 0;
  char associations[MAX_ASSOCIATIONS][9] = {};
};

struct YapPackageInfo {
  static constexpr uint8_t MAX_SECTIONS = 16;

  char path[129] = {};
  uint32_t fileSize = 0;
  uint16_t sectionCount = 0;
  YapSection sections[MAX_SECTIONS];
  YapManifest manifest;
};

class YapPackageService {
 public:
  static constexpr uint16_t FORMAT_VERSION = 1;
  static constexpr uint8_t API_MAJOR = 1;
  static constexpr uint32_t MIN_MEMORY = 16u * 1024u;
  static constexpr uint32_t MAX_MEMORY = 96u * 1024u;
  static constexpr uint32_t MAX_PACKAGE_SIZE = 8u * 1024u * 1024u;

  void begin(StorageService& storage, Logger& logger) {
    storage_ = &storage;
    logger_ = &logger;
  }
  YapError inspect(const char* path, YapPackageInfo& info);
  static const char* errorCode(YapError error);
  static const char* launchModeName(YapLaunchMode mode);

  static constexpr uint32_t TYPE_MANIFEST =
      static_cast<uint32_t>('M') | (static_cast<uint32_t>('A') << 8) |
      (static_cast<uint32_t>('N') << 16) | (static_cast<uint32_t>('F') << 24);
  static constexpr uint32_t TYPE_LUA_SOURCE =
      static_cast<uint32_t>('L') | (static_cast<uint32_t>('U') << 8) |
      (static_cast<uint32_t>('A') << 16) | (static_cast<uint32_t>('S') << 24);
  static constexpr uint32_t TYPE_ICON =
      static_cast<uint32_t>('I') | (static_cast<uint32_t>('C') << 8) |
      (static_cast<uint32_t>('O') << 16) | (static_cast<uint32_t>('N') << 24);
  static constexpr uint32_t TYPE_RESOURCE =
      static_cast<uint32_t>('R') | (static_cast<uint32_t>('S') << 8) |
      (static_cast<uint32_t>('R') << 16) | (static_cast<uint32_t>('C') << 24);

 private:
  StorageService* storage_ = nullptr;
  Logger* logger_ = nullptr;

  YapError parseManifest(const char* path, const YapSection& section,
                         const YapPackageInfo& package, YapManifest& manifest);
  static uint16_t read16(const uint8_t* bytes);
  static uint32_t read32(const uint8_t* bytes);
  static bool copyFixedString(const uint8_t* source, size_t width, char* output,
                              size_t outputCapacity, bool utf8);
  static bool validUtf8(const uint8_t* text, size_t length);
  static bool validAppId(const char* value);
  static bool validEntryPoint(const char* value);
  static bool validExtension(const char* value);
};
