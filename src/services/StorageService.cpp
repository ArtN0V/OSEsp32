#include "StorageService.h"

#include <algorithm>
#include <strings.h>
#include <new>

#include "../board/BoardConfig.h"

StorageService::StorageService() : spi_(VSPI) {}

void StorageService::begin(EventBus& events, Logger& logger) {
  events_ = &events;
  logger_ = &logger;
  mount();
}

bool StorageService::mount() {
  if (!spiStarted_) {
    spi_.begin(board::SD_SCLK, board::SD_MISO, board::SD_MOSI, board::SD_CS);
    spiStarted_ = true;
  }
  mounted_ = SD.begin(board::SD_CS, spi_, board::SD_FREQUENCY);
  lastProbeMs_ = millis();
  if (mounted_) {
    ++generation_;
    ensureSystemDirectories();
    unavailableReported_ = false;
    logger_->info("storage", "SD mounted: %llu MiB",
                  static_cast<unsigned long long>(SD.cardSize() / 1048576));
    events_->publish(SystemEventType::StorageMounted,
                     static_cast<uint32_t>(SD.cardSize() / 1048576));
  } else {
    if (!unavailableReported_) {
      unavailableReported_ = true;
      logger_->warning("storage", "SD card unavailable");
      events_->publish(SystemEventType::StorageError);
    }
  }
  return mounted_;
}

void StorageService::ensureSystemDirectories() {
  static constexpr const char* directories[] = {
      "/OSEsp32", "/OSEsp32/Apps", "/OSEsp32/Data",
      "/OSEsp32/Wallpapers", "/OSEsp32/Notes", "/OSEsp32/Transactions",
      "/Documents"};
  for (const char* directory : directories) {
    if (!SD.exists(directory) && !SD.mkdir(directory)) {
      logger_->warning("storage", "could not create %s", directory);
    }
  }
}

void StorageService::markRemoved() {
  if (!mounted_) return;
  releaseReadFile();
  releaseWriteFile();
  mounted_ = false;
  ++generation_;
  SD.end();
  logger_->warning("storage", "SD card removed");
  events_->publish(SystemEventType::StorageRemoved);
}

void StorageService::update() {
  const uint32_t now = millis();
  if (now - lastProbeMs_ < 3000) return;
  lastProbeMs_ = now;
  if (mounted_) {
    // Raw sector probes must not overlap an active FAT file operation.
    releaseReadFile();
    releaseWriteFile();
    uint8_t probe[512];
    if (SD.cardType() == CARD_NONE || !SD.readRAW(probe, 0)) markRemoved();
  } else {
    mount();
  }
}

const char* StorageService::normalizePath(const char* path, char* buffer,
                                          size_t bufferSize) {
  if (!path || !buffer || bufferSize < 2) return nullptr;
  size_t outputLength = 0;
  buffer[outputLength++] = '/';
  const char* cursor = path;
  while (*cursor == '/') ++cursor;
  while (*cursor) {
    const char* component = cursor;
    size_t componentLength = 0;
    while (cursor[componentLength] && cursor[componentLength] != '/') {
      const uint8_t character = static_cast<uint8_t>(cursor[componentLength]);
      if (character < 0x20 || character == 0x7F || character == '\\' ||
          character == ':' || character == '*' || character == '?' ||
          character == '"' || character == '<' || character == '>' ||
          character == '|')
        return nullptr;
      ++componentLength;
    }
    if ((componentLength == 1 && component[0] == '.') ||
        (componentLength == 2 && component[0] == '.' && component[1] == '.'))
      return nullptr;
    if (!componentLength) return nullptr;
    if (outputLength > 1) {
      if (outputLength + 1 >= bufferSize) return nullptr;
      buffer[outputLength++] = '/';
    }
    if (outputLength + componentLength >= bufferSize) return nullptr;
    memcpy(buffer + outputLength, component, componentLength);
    outputLength += componentLength;
    cursor += componentLength;
    if (*cursor == '/') {
      ++cursor;
      // A root slash is accepted, but empty interior/trailing components are
      // ambiguous on FAT and must not canonicalize to another path.
      if (!*cursor || *cursor == '/') return nullptr;
    }
  }
  buffer[outputLength] = '\0';
  return buffer;
}

bool StorageService::listDirectoryPage(const char* path, uint16_t offset,
                                       StorageEntry* entries, uint8_t capacity,
                                       uint8_t& count, uint16_t& totalCount) {
  count = 0;
  totalCount = 0;
  if (!mounted_ || !entries || capacity == 0) return false;
  char normalized[129];
  if (!normalizePath(path, normalized, sizeof(normalized))) return false;
  File directory = SD.open(normalized, FILE_READ);
  if (!directory || !directory.isDirectory()) {
    if (directory) directory.close();
    return false;
  }

  File file = directory.openNextFile(FILE_READ);
  while (file) {
    // A truncated path can point to a different file. Never publish it.
    if (strlen(file.name())>=sizeof(entries[0].name) || strlen(file.path())>=sizeof(entries[0].path)) {
      file.close(); file=directory.openNextFile(FILE_READ); continue;
    }
    if (totalCount >= offset && count < capacity) {
      StorageEntry& entry = entries[count++];
      strlcpy(entry.name, file.name(), sizeof(entry.name));
      strlcpy(entry.path, file.path(), sizeof(entry.path));
      entry.size = static_cast<uint32_t>(file.size());
      entry.directory = file.isDirectory();
    }
    if (totalCount==UINT16_MAX) { file.close(); break; }
    ++totalCount;
    file.close();
    file = directory.openNextFile(FILE_READ);
  }
  directory.close();
  return true;
}

bool StorageService::exists(const char* path) const {
  char normalized[129];
  return mounted_ && normalizePath(path, normalized, sizeof(normalized)) &&
         SD.exists(normalized);
}

bool StorageService::recoverBuiltinReplacement(const char* path) {
  char target[129];
  if (!mounted_ || !normalizePath(path,target,sizeof(target))) return false;
  const char* name=target+min(strlen(target),static_cast<size_t>(15));
  const char* ext=strrchr(name,'.');
  const bool note=!strncmp(target,"/OSEsp32/Notes/",15) && *name && !strchr(name,'/') && ext && !strcasecmp(ext,".note");
  if (!note && strcmp(target,"/OSEsp32/Wallpapers/desktop.owp")) return false;
  char backup[145]; snprintf(backup,sizeof(backup),"%s.bak",target);
  if (!SD.exists(backup)) return true;
  // Only our known built-in transaction destinations are eligible. A target
  // installed by rename was fully written before the backup was made.
  if (!SD.exists(target)) return SD.rename(backup,target);
  return SD.remove(backup);
}

bool StorageService::removePath(const char* path) {
  releaseReadFile();
  releaseWriteFile();
  char normalized[129];
  return mounted_ && normalizePath(path, normalized, sizeof(normalized)) &&
         strcmp(normalized, "/") &&
         (!SD.exists(normalized) || SD.remove(normalized));
}

bool StorageService::renamePath(const char* from, const char* to) {
  releaseReadFile();
  releaseWriteFile();
  char normalizedFrom[129];
  char normalizedTo[129];
  if (!mounted_ ||
      !normalizePath(from, normalizedFrom, sizeof(normalizedFrom)) ||
      !normalizePath(to, normalizedTo, sizeof(normalizedTo)) ||
      !strcmp(normalizedFrom, "/") || !strcmp(normalizedTo, "/") ||
      !strcmp(normalizedFrom, normalizedTo) || !SD.exists(normalizedFrom) ||
      SD.exists(normalizedTo))
    return false;
  return SD.rename(normalizedFrom, normalizedTo);
}

bool StorageService::replacePathAtomic(const char* completedTemporary,
                                       const char* destination) {
  releaseReadFile();
  releaseWriteFile();
  char temporary[145];
  char target[129];
  if (!mounted_ ||
      !normalizePath(completedTemporary, temporary, sizeof(temporary)) ||
      !normalizePath(destination, target, sizeof(target)) ||
      !strcmp(temporary, "/") || !strcmp(target, "/") ||
      !strcmp(temporary, target) || !SD.exists(temporary))
    return false;
  char backup[145];
  if (snprintf(backup, sizeof(backup), "%s.bak", target) >=
      static_cast<int>(sizeof(backup)))
    return false;
  // Never destroy the only remaining old copy from an interrupted replace.
  if (SD.exists(backup)) {
    if (!SD.exists(target)) {
      if (!SD.rename(backup, target)) return false;
    } else {
      return false; // Preserve ambiguous remnants for recovery/manual review.
    }
  }
  const bool hadOriginal = SD.exists(target);
  if (hadOriginal && !SD.rename(target, backup)) return false;
  if (!SD.rename(temporary, target)) {
    if (hadOriginal) SD.rename(backup, target);
    return false;
  }
  if (hadOriginal) SD.remove(backup);
  return true;
}

bool StorageService::readFile(const char* path, char* buffer, size_t capacity,
                              size_t& length, bool allowTruncate) {
  releaseReadFile();
  releaseWriteFile(path);
  length = 0;
  if (!mounted_ || !path || !buffer || capacity < 2) return false;
  char normalized[129];
  if (!normalizePath(path, normalized, sizeof(normalized))) return false;
  File file = SD.open(normalized, FILE_READ);
  if (!file || file.isDirectory()) {
    if (file) file.close();
    return false;
  }
  const size_t fileSize = static_cast<size_t>(file.size());
  if (!allowTruncate && fileSize >= capacity) {
    file.close();
    return false;
  }
  const size_t wanted = std::min(fileSize, capacity - 1);
  length = file.readBytes(buffer, wanted);
  file.close();
  buffer[length] = '\0';
  return length == wanted;
}

bool StorageService::makeDirectory(const char* path) {
  char normalized[129];
  if (!mounted_ || !normalizePath(path, normalized, sizeof(normalized))) return false;
  if (!SD.exists(normalized)) return SD.mkdir(normalized);
  File file = SD.open(normalized, FILE_READ);
  const bool directory = file && file.isDirectory();
  file.close();
  return directory;
}

uint64_t StorageService::freeBytes() const {
  if (!mounted_) return 0;
  const uint64_t total = SD.totalBytes(), used = SD.usedBytes();
  return total > used ? total - used : 0;
}

bool StorageService::writeRange(const char* path, uint32_t offset,
                                const uint8_t* data, size_t length, bool truncate) {
  releaseReadFile();
  char normalized[129];
  if (!mounted_ || length > 512 || (!data && length) ||
      !normalizePath(path, normalized, sizeof(normalized))) return false;
  if (truncate || !rangeWriteFile_ || strcasecmp(rangeWritePath_,normalized)) {
    releaseWriteFile();
    rangeWriteFile_=SD.open(normalized,truncate ? "w" : "r+");
    if (rangeWriteFile_ && !rangeWriteFile_.isDirectory()) {
      strlcpy(rangeWritePath_,normalized,sizeof(rangeWritePath_));
      rangeWriteSize_=truncate ? 0 : static_cast<uint32_t>(rangeWriteFile_.size());
    }
  }
  if (!rangeWriteFile_ || rangeWriteFile_.isDirectory()) {
    releaseWriteFile(); return false;
  }
  const bool ok=offset<=rangeWriteSize_ && rangeWriteFile_.seek(offset) &&
                (!length || rangeWriteFile_.write(data,length)==length);
  if (ok) rangeWriteSize_=max(rangeWriteSize_,offset+static_cast<uint32_t>(length));
  else releaseWriteFile();
  return ok;
}

bool StorageService::fileSize(const char* path, uint32_t& size) const {
  size = 0;
  if (!mounted_ || !path) return false;
  char normalized[129];
  if (!normalizePath(path, normalized, sizeof(normalized))) return false;
  File file = SD.open(normalized, FILE_READ);
  if (!file || file.isDirectory()) {
    if (file) file.close();
    return false;
  }
  size = static_cast<uint32_t>(file.size());
  file.close();
  return true;
}

bool StorageService::readFileRange(const char* path, uint32_t offset,
                                   uint8_t* buffer, size_t length,
                                   size_t& bytesRead) const {
  bytesRead = 0;
  if (!mounted_ || !path || (!buffer && length)) return false;
  char normalized[129];
  if (!normalizePath(path, normalized, sizeof(normalized))) return false;
  releaseWriteFile(normalized);
  if (!rangeReadFile_ || strcasecmp(rangeReadPath_,normalized)) {
    releaseReadFile();
    rangeReadFile_=SD.open(normalized,FILE_READ);
    if (rangeReadFile_ && !rangeReadFile_.isDirectory())
      strlcpy(rangeReadPath_,normalized,sizeof(rangeReadPath_));
  }
  if (!rangeReadFile_ || rangeReadFile_.isDirectory()) {
    releaseReadFile();
    return false;
  }
  if (offset > static_cast<uint32_t>(rangeReadFile_.size()) ||
      !rangeReadFile_.seek(offset)) {
    releaseReadFile();
    return false;
  }
  bytesRead=rangeReadFile_.read(buffer,length);
  if (bytesRead!=length) { releaseReadFile(); return false; }
  return true;
}

void StorageService::releaseReadFile(const char* path) const {
  if (path && *rangeReadPath_ && strcasecmp(path,rangeReadPath_)) return;
  if (rangeReadFile_) rangeReadFile_.close();
  rangeReadFile_=File();
  rangeReadPath_[0]=0;
}

void StorageService::releaseWriteFile(const char* path) const {
  if (path && *rangeWritePath_ && strcasecmp(path,rangeWritePath_)) return;
  if (rangeWriteFile_) {
    rangeWriteFile_.flush();
    rangeWriteFile_.close();
  }
  rangeWriteFile_=File();
  rangeWritePath_[0]=0;
  rangeWriteSize_=0;
}

bool StorageService::computeFileCrc32(const char* path, uint32_t offset,
                                      uint32_t length, uint32_t& crc,
                                      uint32_t zeroOffset,
                                      uint32_t zeroLength) const {
  releaseReadFile();
  releaseWriteFile(path);
  crc = 0;
  if (!mounted_ || !path) return false;
  char normalized[129];
  if (!normalizePath(path, normalized, sizeof(normalized))) return false;
  File file = SD.open(normalized, FILE_READ);
  if (!file || file.isDirectory()) {
    if (file) file.close();
    return false;
  }
  const uint32_t size = static_cast<uint32_t>(file.size());
  if (offset > size || length > size - offset || !file.seek(offset)) {
    file.close();
    return false;
  }
  uint8_t buffer[256];
  uint32_t value = 0xFFFFFFFFu;
  uint32_t consumed = 0;
  while (consumed < length) {
    const size_t wanted = min(static_cast<uint32_t>(sizeof(buffer)),
                              length - consumed);
    const size_t received = file.read(buffer, wanted);
    if (received != wanted) {
      file.close();
      return false;
    }
    for (size_t index = 0; index < received; ++index) {
      const uint32_t absolute = offset + consumed + index;
      uint8_t byte = buffer[index];
      if (zeroOffset != UINT32_MAX && absolute >= zeroOffset &&
          absolute - zeroOffset < zeroLength)
        byte = 0;
      value ^= byte;
      for (uint8_t bit = 0; bit < 8; ++bit)
        value = (value >> 1) ^
                (0xEDB88320u & static_cast<uint32_t>(
                                      -static_cast<int32_t>(value & 1u)));
    }
    consumed += received;
    if (!(consumed%4096)) delay(1); // Let RTOS housekeeping run during large CRC scans.
  }
  file.close();
  crc = value ^ 0xFFFFFFFFu;
  return true;
}

bool StorageService::writeFileAtomic(const char* path, const uint8_t* data,
                                     size_t length) {
  releaseReadFile();
  releaseWriteFile();
  if (!mounted_ || !path || (!data && length) || !path[0]) return false;
  char normalized[129];
  if (!normalizePath(path, normalized, sizeof(normalized)) ||
      !strcmp(normalized, "/"))
    return false;
  char temporary[145];
  if (snprintf(temporary, sizeof(temporary), "%s.tmp", normalized) >=
      static_cast<int>(sizeof(temporary)))
    return false;

  if (SD.exists(temporary)) SD.remove(temporary);
  File file = SD.open(temporary, FILE_WRITE);
  if (!file) return false;
  const size_t written = file.write(data, length);
  file.flush();
  file.close();
  if (written != length) {
    SD.remove(temporary);
    return false;
  }

  if (!replacePathAtomic(temporary, normalized)) {
    SD.remove(temporary);
    return false;
  }
  return true;
}

bool StorageService::isImagePath(const char* path) {
  if (!path) return false;
  const char* extension = strrchr(path, '.');
  return extension && (!strcasecmp(extension, ".bmp") ||
                       !strcasecmp(extension, ".jpg") ||
                       !strcasecmp(extension, ".jpeg"));
}

bool StorageService::isYapPath(const char* path) {
  if (!path) return false;
  const char* extension = strrchr(path, '.');
  return extension && !strcasecmp(extension, ".yap");
}

bool StorageService::makeLvglPath(const char* sdPath, char* output,
                                  size_t outputSize) {
  if (!sdPath || !output || outputSize < 4) return false;
  const int written = snprintf(output, outputSize, "S:%s%s",
                               sdPath[0] == '/' ? "" : "/", sdPath);
  if (written <= 0 || static_cast<size_t>(written) >= outputSize) return false;

  // LVGL's built-in BMP and TJPGD decoders compare extensions
  // case-sensitively. FAT paths are case-insensitive, so normalize only the
  // extension presented to LVGL while preserving the stored SD path.
  char* extension = strrchr(output, '.');
  if (extension) {
    for (char* character = extension + 1; *character; ++character) {
      if (*character >= 'A' && *character <= 'Z')
        *character = static_cast<char>(*character - 'A' + 'a');
    }
  }
  return true;
}

void StorageService::registerLvglDriver() {
  if (lvglRegistered_) return;
  lv_fs_drv_init(&lvglDriver_);
  lvglDriver_.letter = 'S';
  lvglDriver_.cache_size = 512;
  lvglDriver_.user_data = this;
  lvglDriver_.ready_cb = readyCallback;
  lvglDriver_.open_cb = openCallback;
  lvglDriver_.close_cb = closeCallback;
  lvglDriver_.read_cb = readCallback;
  lvglDriver_.write_cb = writeCallback;
  lvglDriver_.seek_cb = seekCallback;
  lvglDriver_.tell_cb = tellCallback;
  lvglDriver_.dir_open_cb = directoryOpenCallback;
  lvglDriver_.dir_read_cb = directoryReadCallback;
  lvglDriver_.dir_close_cb = directoryCloseCallback;
  lv_fs_drv_register(&lvglDriver_);
  lvglRegistered_ = true;
}

bool StorageService::readyCallback(lv_fs_drv_t* driver) {
  return static_cast<StorageService*>(driver->user_data)->mounted_;
}

void* StorageService::openCallback(lv_fs_drv_t* driver, const char* path,
                                   lv_fs_mode_t mode) {
  StorageService* service = static_cast<StorageService*>(driver->user_data);
  if (!service->mounted_) return nullptr;
  char normalized[129];
  if (!normalizePath(path, normalized, sizeof(normalized))) return nullptr;
  const char* openMode = (mode & LV_FS_MODE_WR) ? FILE_WRITE : FILE_READ;
  File* file = new (std::nothrow) File(SD.open(normalized, openMode));
  if (!*file) {
    delete file;
    return nullptr;
  }
  return file;
}

lv_fs_res_t StorageService::closeCallback(lv_fs_drv_t*, void* filePointer) {
  File* file = static_cast<File*>(filePointer);
  file->close();
  delete file;
  return LV_FS_RES_OK;
}

lv_fs_res_t StorageService::readCallback(lv_fs_drv_t*, void* filePointer,
                                         void* buffer, uint32_t bytesToRead,
                                         uint32_t* bytesRead) {
  File* file = static_cast<File*>(filePointer);
  const size_t read = file->read(static_cast<uint8_t*>(buffer), bytesToRead);
  if (bytesRead) *bytesRead = static_cast<uint32_t>(read);
  return LV_FS_RES_OK;
}

lv_fs_res_t StorageService::writeCallback(lv_fs_drv_t*, void* filePointer,
                                          const void* buffer,
                                          uint32_t bytesToWrite,
                                          uint32_t* bytesWritten) {
  File* file = static_cast<File*>(filePointer);
  const size_t written = file->write(static_cast<const uint8_t*>(buffer),
                                     bytesToWrite);
  if (bytesWritten) *bytesWritten = static_cast<uint32_t>(written);
  return written == bytesToWrite ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

lv_fs_res_t StorageService::seekCallback(lv_fs_drv_t*, void* filePointer,
                                         uint32_t position,
                                         lv_fs_whence_t whence) {
  SeekMode mode = SeekSet;
  if (whence == LV_FS_SEEK_CUR) mode = SeekCur;
  if (whence == LV_FS_SEEK_END) mode = SeekEnd;
  return static_cast<File*>(filePointer)->seek(position, mode)
             ? LV_FS_RES_OK
             : LV_FS_RES_FS_ERR;
}

lv_fs_res_t StorageService::tellCallback(lv_fs_drv_t*, void* filePointer,
                                         uint32_t* position) {
  if (position)
    *position = static_cast<uint32_t>(
        static_cast<File*>(filePointer)->position());
  return LV_FS_RES_OK;
}

void* StorageService::directoryOpenCallback(lv_fs_drv_t* driver,
                                            const char* path) {
  StorageService* service = static_cast<StorageService*>(driver->user_data);
  if (!service->mounted_) return nullptr;
  char normalized[129];
  if (!normalizePath(path, normalized, sizeof(normalized))) return nullptr;
  File* directory = new (std::nothrow) File(SD.open(normalized, FILE_READ));
  if (!*directory || !directory->isDirectory()) {
    if (*directory) directory->close();
    delete directory;
    return nullptr;
  }
  return directory;
}

lv_fs_res_t StorageService::directoryReadCallback(lv_fs_drv_t*,
                                                  void* directoryPointer,
                                                  char* name,
                                                  uint32_t nameLength) {
  File file = static_cast<File*>(directoryPointer)->openNextFile(FILE_READ);
  if (!file) {
    if (nameLength) name[0] = '\0';
    return LV_FS_RES_OK;
  }
  snprintf(name, nameLength, "%s%s", file.isDirectory() ? "/" : "",
           file.name());
  file.close();
  return LV_FS_RES_OK;
}

lv_fs_res_t StorageService::directoryCloseCallback(lv_fs_drv_t*,
                                                   void* directoryPointer) {
  File* directory = static_cast<File*>(directoryPointer);
  directory->close();
  delete directory;
  return LV_FS_RES_OK;
}
