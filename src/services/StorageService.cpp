#include "StorageService.h"

#include <algorithm>
#include <strings.h>

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
      "/OSEsp32/Wallpapers", "/OSEsp32/Notes"};
  for (const char* directory : directories) {
    if (!SD.exists(directory) && !SD.mkdir(directory)) {
      logger_->warning("storage", "could not create %s", directory);
    }
  }
}

void StorageService::markRemoved() {
  if (!mounted_) return;
  mounted_ = false;
  SD.end();
  logger_->warning("storage", "SD card removed");
  events_->publish(SystemEventType::StorageRemoved);
}

void StorageService::update() {
  const uint32_t now = millis();
  if (now - lastProbeMs_ < 3000) return;
  lastProbeMs_ = now;
  if (mounted_) {
    if (SD.cardType() == CARD_NONE) markRemoved();
  } else {
    mount();
  }
}

const char* StorageService::normalizePath(const char* path, char* buffer,
                                          size_t bufferSize) {
  if (!path || !buffer || bufferSize < 2) return nullptr;
  if (strstr(path, "..")) return nullptr;
  if (path[0] == '/') {
    strlcpy(buffer, path, bufferSize);
  } else {
    buffer[0] = '/';
    strlcpy(buffer + 1, path, bufferSize - 1);
  }
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
    if (totalCount >= offset && count < capacity) {
      StorageEntry& entry = entries[count++];
      strlcpy(entry.name, file.name(), sizeof(entry.name));
      strlcpy(entry.path, file.path(), sizeof(entry.path));
      entry.size = static_cast<uint32_t>(file.size());
      entry.directory = file.isDirectory();
    }
    ++totalCount;
    file.close();
    file = directory.openNextFile(FILE_READ);
  }
  directory.close();
  return true;
}

bool StorageService::exists(const char* path) const {
  return mounted_ && path && SD.exists(path);
}

bool StorageService::removePath(const char* path) {
  return mounted_ && path && (!SD.exists(path) || SD.remove(path));
}

bool StorageService::renamePath(const char* from, const char* to) {
  if (!mounted_ || !from || !to || !SD.exists(from)) return false;
  if (SD.exists(to) && !SD.remove(to)) return false;
  return SD.rename(from, to);
}

bool StorageService::readFile(const char* path, char* buffer, size_t capacity,
                              size_t& length, bool allowTruncate) {
  length = 0;
  if (!mounted_ || !path || !buffer || capacity < 2) return false;
  File file = SD.open(path, FILE_READ);
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

bool StorageService::writeFileAtomic(const char* path, const uint8_t* data,
                                     size_t length) {
  if (!mounted_ || !path || !data || !path[0]) return false;
  char temporary[145];
  char backup[145];
  if (snprintf(temporary, sizeof(temporary), "%s.tmp", path) >=
          static_cast<int>(sizeof(temporary)) ||
      snprintf(backup, sizeof(backup), "%s.bak", path) >=
          static_cast<int>(sizeof(backup)))
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

  if (SD.exists(backup) && !SD.remove(backup)) {
    SD.remove(temporary);
    return false;
  }
  const bool hadOriginal = SD.exists(path);
  if (hadOriginal && !SD.rename(path, backup)) {
    SD.remove(temporary);
    return false;
  }
  if (!SD.rename(temporary, path)) {
    if (hadOriginal) SD.rename(backup, path);
    SD.remove(temporary);
    return false;
  }
  if (hadOriginal) SD.remove(backup);
  return true;
}

bool StorageService::isImagePath(const char* path) {
  if (!path) return false;
  const char* extension = strrchr(path, '.');
  return extension && (!strcasecmp(extension, ".bmp") ||
                       !strcasecmp(extension, ".jpg") ||
                       !strcasecmp(extension, ".jpeg"));
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
  File* file = new File(SD.open(normalized, openMode));
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
  File* directory = new File(SD.open(normalized, FILE_READ));
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
