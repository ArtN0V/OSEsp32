#include "YapPackageService.h"

#include <ctype.h>
#include <string.h>

namespace {
constexpr uint16_t HEADER_SIZE = 32;
constexpr uint16_t SECTION_SIZE = 20;
constexpr uint16_t MANIFEST_SIZE = 160;
constexpr uint32_t PACKAGE_CRC_OFFSET = 20;
constexpr uint32_t KNOWN_CAPABILITIES =
    YapPrivateRead | YapPrivateWrite | YapDocumentsOpen |
    YapDocumentsCreate | YapDocumentsReplace;

bool addOverflows(uint32_t left, uint32_t right) {
  return right > UINT32_MAX - left;
}
}  // namespace

uint16_t YapPackageService::read16(const uint8_t* bytes) {
  return static_cast<uint16_t>(bytes[0]) |
         (static_cast<uint16_t>(bytes[1]) << 8);
}

uint32_t YapPackageService::read32(const uint8_t* bytes) {
  return static_cast<uint32_t>(bytes[0]) |
         (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) |
         (static_cast<uint32_t>(bytes[3]) << 24);
}

bool YapPackageService::validUtf8(const uint8_t* text, size_t length) {
  for (size_t index = 0; index < length;) {
    const uint8_t first = text[index++];
    if (first < 0x80) {
      if (first < 0x20 || first == 0x7F) return false;
      continue;
    }
    uint8_t continuationCount = 0;
    uint32_t codePoint = 0;
    uint32_t minimum = 0;
    if ((first & 0xE0) == 0xC0) {
      continuationCount = 1;
      codePoint = first & 0x1F;
      minimum = 0x80;
    } else if ((first & 0xF0) == 0xE0) {
      continuationCount = 2;
      codePoint = first & 0x0F;
      minimum = 0x800;
    } else if ((first & 0xF8) == 0xF0) {
      continuationCount = 3;
      codePoint = first & 0x07;
      minimum = 0x10000;
    } else {
      return false;
    }
    if (index + continuationCount > length) return false;
    while (continuationCount--) {
      const uint8_t next = text[index++];
      if ((next & 0xC0) != 0x80) return false;
      codePoint = (codePoint << 6) | (next & 0x3F);
    }
    if (codePoint < minimum || codePoint > 0x10FFFF ||
        (codePoint >= 0xD800 && codePoint <= 0xDFFF))
      return false;
  }
  return true;
}

bool YapPackageService::copyFixedString(const uint8_t* source, size_t width,
                                        char* output, size_t outputCapacity,
                                        bool utf8) {
  if (!source || !output || !outputCapacity || outputCapacity <= width)
    return false;
  size_t length = 0;
  while (length < width && source[length]) ++length;
  if (!length || length == width) return false;
  if (utf8 && !validUtf8(source, length)) return false;
  memcpy(output, source, length);
  output[length] = '\0';
  return true;
}

bool YapPackageService::validAppId(const char* value) {
  if (!value || !isalnum(static_cast<unsigned char>(value[0]))) return false;
  size_t length = 0;
  for (; value[length]; ++length) {
    const unsigned char character = value[length];
    if (!(character >= 'a' && character <= 'z') &&
        !(character >= '0' && character <= '9') && character != '.' &&
        character != '_' && character != '-')
      return false;
  }
  return length <= 31 && value[length - 1] != '.';
}

bool YapPackageService::validEntryPoint(const char* value) {
  if (!value || !(isalpha(static_cast<unsigned char>(value[0])) ||
                  value[0] == '_'))
    return false;
  for (size_t index = 1; value[index]; ++index)
    if (!(isalnum(static_cast<unsigned char>(value[index])) ||
          value[index] == '_'))
      return false;
  return true;
}

bool YapPackageService::validExtension(const char* value) {
  if (!value || !value[0]) return false;
  for (size_t index = 0; value[index]; ++index)
    if (!(value[index] >= 'a' && value[index] <= 'z') &&
        !(value[index] >= '0' && value[index] <= '9'))
      return false;
  return true;
}

YapError YapPackageService::parseManifest(const char* path,
                                          const YapSection& section,
                                          const YapPackageInfo& package,
                                          YapManifest& manifest) {
  if (section.length != MANIFEST_SIZE) return YapError::BadManifest;
  uint8_t bytes[MANIFEST_SIZE];
  size_t received = 0;
  if (!storage_->readFileRange(path, section.offset, bytes, sizeof(bytes),
                               received))
    return YapError::IoError;
  if (memcmp(bytes, "MNF1", 4) || read16(bytes + 4) != MANIFEST_SIZE ||
      bytes[6] != 1 || bytes[11] != 0)
    return YapError::BadManifest;
  if (bytes[7] > static_cast<uint8_t>(YapLaunchMode::Exclusive) ||
      bytes[10] > YapManifest::MAX_ASSOCIATIONS)
    return YapError::BadManifest;
  manifest.launchMode = static_cast<YapLaunchMode>(bytes[7]);
  manifest.apiMajor = bytes[8];
  manifest.apiMinor = bytes[9];
  manifest.associationCount = bytes[10];
  manifest.requestedMemory = read32(bytes + 12);
  manifest.capabilities = read32(bytes + 16);
  manifest.codeSection = read16(bytes + 20);
  manifest.iconSection = read16(bytes + 22);
  if (manifest.apiMajor != API_MAJOR) return YapError::UnsupportedApi;
  if (manifest.requestedMemory < MIN_MEMORY ||
      manifest.requestedMemory > MAX_MEMORY)
    return YapError::BadManifest;
  if (manifest.capabilities & ~KNOWN_CAPABILITIES)
    return YapError::UnsupportedCapability;
  if (manifest.codeSection >= package.sectionCount ||
      package.sections[manifest.codeSection].type != TYPE_LUA_SOURCE)
    return YapError::BadManifest;
  if (manifest.iconSection != UINT16_MAX &&
      (manifest.iconSection >= package.sectionCount ||
       package.sections[manifest.iconSection].type != TYPE_ICON))
    return YapError::BadManifest;
  if (!copyFixedString(bytes + 24, 32, manifest.appId,
                       sizeof(manifest.appId), false) ||
      !validAppId(manifest.appId) ||
      !copyFixedString(bytes + 56, 48, manifest.name,
                       sizeof(manifest.name), true) ||
      !copyFixedString(bytes + 104, 24, manifest.entryPoint,
                       sizeof(manifest.entryPoint), false) ||
      !validEntryPoint(manifest.entryPoint))
    return YapError::BadManifest;
  for (uint8_t index = 0; index < manifest.associationCount; ++index) {
    if (!copyFixedString(bytes + 128 + index * 8, 8,
                         manifest.associations[index],
                         sizeof(manifest.associations[index]), false) ||
        !validExtension(manifest.associations[index]))
      return YapError::BadManifest;
  }
  if (manifest.associationCount &&
      !(manifest.capabilities & (YapDocumentsOpen | YapDocumentsCreate)))
    return YapError::BadManifest;
  return YapError::None;
}

YapError YapPackageService::inspect(const char* path, YapPackageInfo& info) {
  info = {};
  if (!storage_ || !storage_->mounted())
    return YapError::StorageUnavailable;
  if (!path || !StorageService::isYapPath(path)) return YapError::BadHeader;
  uint32_t actualSize = 0;
  if (!storage_->fileSize(path, actualSize)) return YapError::IoError;
  if (actualSize < HEADER_SIZE + 2 * SECTION_SIZE + MANIFEST_SIZE)
    return YapError::TooSmall;
  if (actualSize > MAX_PACKAGE_SIZE) return YapError::TooLarge;

  uint8_t header[HEADER_SIZE];
  size_t received = 0;
  if (!storage_->readFileRange(path, 0, header, sizeof(header), received))
    return YapError::IoError;
  if (memcmp(header, "YAP1", 4)) return YapError::BadMagic;
  if (read16(header + 4) != FORMAT_VERSION)
    return YapError::UnsupportedVersion;
  const uint16_t headerSize = read16(header + 6);
  const uint32_t declaredSize = read32(header + 8);
  const uint32_t tableOffset = read32(header + 12);
  const uint16_t sectionCount = read16(header + 16);
  const uint16_t manifestIndex = read16(header + 18);
  const uint32_t expectedPackageCrc = read32(header + 20);
  if (headerSize != HEADER_SIZE || declaredSize != actualSize ||
      tableOffset != HEADER_SIZE || sectionCount < 2 ||
      sectionCount > YapPackageInfo::MAX_SECTIONS ||
      manifestIndex >= sectionCount || read32(header + 24) != 0 ||
      read32(header + 28) != 0)
    return YapError::BadHeader;
  const uint32_t tableLength = sectionCount * SECTION_SIZE;
  if (addOverflows(tableOffset, tableLength) ||
      tableOffset + tableLength > actualSize)
    return YapError::BadSectionTable;

  uint32_t packageCrc = 0;
  if (!storage_->computeFileCrc32(path, 0, actualSize, packageCrc,
                                  PACKAGE_CRC_OFFSET, 4))
    return YapError::IoError;
  if (packageCrc != expectedPackageCrc) return YapError::BadPackageCrc;

  info.fileSize = actualSize;
  info.sectionCount = sectionCount;
  strlcpy(info.path, path, sizeof(info.path));
  uint8_t entry[SECTION_SIZE];
  uint8_t manifestCount = 0;
  uint8_t codeCount = 0;
  uint8_t iconCount = 0;
  const uint32_t payloadStart = tableOffset + tableLength;
  for (uint16_t index = 0; index < sectionCount; ++index) {
    const uint32_t entryOffset = tableOffset + index * SECTION_SIZE;
    if (!storage_->readFileRange(path, entryOffset, entry, sizeof(entry),
                                 received))
      return YapError::IoError;
    YapSection& section = info.sections[index];
    section.type = read32(entry);
    section.offset = read32(entry + 4);
    section.length = read32(entry + 8);
    section.crc32 = read32(entry + 12);
    section.flags = read32(entry + 16);
    if (!section.length || section.flags || section.offset < payloadStart ||
        (section.offset & 3u) || addOverflows(section.offset, section.length) ||
        section.offset + section.length > actualSize)
      return YapError::BadSectionBounds;
    if (section.type == TYPE_MANIFEST)
      ++manifestCount;
    else if (section.type == TYPE_LUA_SOURCE)
      ++codeCount;
    else if (section.type == TYPE_ICON)
      ++iconCount;
    else if (section.type != TYPE_RESOURCE)
      return YapError::BadSectionTable;
    for (uint16_t previous = 0; previous < index; ++previous) {
      const YapSection& other = info.sections[previous];
      if (section.offset < other.offset + other.length &&
          other.offset < section.offset + section.length)
        return YapError::OverlappingSections;
    }
    uint32_t sectionCrc = 0;
    if (!storage_->computeFileCrc32(path, section.offset, section.length,
                                    sectionCrc))
      return YapError::IoError;
    if (sectionCrc != section.crc32) return YapError::BadSectionCrc;
  }
  if (manifestCount != 1 || codeCount != 1 || iconCount > 1)
    return YapError::DuplicateSection;
  if (info.sections[manifestIndex].type != TYPE_MANIFEST)
    return YapError::BadManifest;
  const YapError manifestError =
      parseManifest(path, info.sections[manifestIndex], info, info.manifest);
  if (manifestError != YapError::None) return manifestError;
  if (logger_)
    logger_->info("yap", "validated %s id=%s sections=%u bytes=%lu", path,
                  info.manifest.appId, info.sectionCount,
                  static_cast<unsigned long>(info.fileSize));
  return YapError::None;
}

const char* YapPackageService::errorCode(YapError error) {
  switch (error) {
    case YapError::None: return "ok";
    case YapError::StorageUnavailable: return "storage_unavailable";
    case YapError::IoError: return "io_error";
    case YapError::TooSmall: return "package_too_small";
    case YapError::TooLarge: return "package_too_large";
    case YapError::BadMagic: return "bad_magic";
    case YapError::UnsupportedVersion: return "unsupported_version";
    case YapError::BadHeader: return "bad_header";
    case YapError::BadPackageCrc: return "bad_package_crc";
    case YapError::BadSectionTable: return "bad_section_table";
    case YapError::BadSectionBounds: return "bad_section_bounds";
    case YapError::OverlappingSections: return "overlapping_sections";
    case YapError::DuplicateSection: return "duplicate_section";
    case YapError::BadSectionCrc: return "bad_section_crc";
    case YapError::BadManifest: return "bad_manifest";
    case YapError::UnsupportedApi: return "unsupported_api";
    case YapError::UnsupportedCapability: return "unsupported_capability";
  }
  return "unknown";
}

const char* YapPackageService::launchModeName(YapLaunchMode mode) {
  switch (mode) {
    case YapLaunchMode::Windowed: return "windowed";
    case YapLaunchMode::Fullscreen: return "fullscreen";
    case YapLaunchMode::Exclusive: return "exclusive";
  }
  return "unknown";
}
