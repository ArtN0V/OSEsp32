#pragma once
#include "YapPackageService.h"

// Session capabilities are integer tokens, never native File or SD paths.
class AppStorageService {
 public:
  static constexpr uint8_t MAX_HANDLES = 4;
  static constexpr uint32_t MAX_FILE_SIZE = 1024 * 1024;
  static constexpr uint32_t RESERVE = 128 * 1024;
  void begin(StorageService& storage, const YapPackageInfo& package);
  void end();
  void invalidate();
  static bool recover(StorageService& storage);
  int open(const char* path, const char* mode);
  int grant(const char* selectedPath, const char* mode); // Trusted picker only.
  bool read(int token, uint8_t* data, size_t wanted, size_t& count);
  bool write(int token, const uint8_t* data, size_t count);
  bool seek(int token, uint32_t position);
  bool size(int token, uint32_t& length);
  bool close(int token, bool commit = true);
  bool flush(int token);
  bool stat(const char* path, uint32_t& length);
  bool list(const char* path, uint16_t page, StorageEntry* entries, uint8_t& count);
  bool mkdir(const char* path);
  const char* error() const { return error_; }
  bool permitsDocument(const char* path, const char* mode) const;
  static bool validRelative(const char* path, bool emptyAllowed = false);
 private:
  struct Handle {
    int token = 0;
    bool writable = false;
    bool createOnly = false;
    uint32_t cursor = 0, length = 0, base = 0;
    char path[129] = {};
    char target[129] = {};
  };
  StorageService* storage_ = nullptr;
  YapPackageInfo package_;
  Handle handles_[MAX_HANDLES];
  uint32_t generation_ = 0;
  int nextToken_ = 1;
  const char* error_ = "ok";
  bool fail(const char* message) { error_ = message; return false; }
  bool ready();
  Handle* handle(int token);
  int openResolved(const char* path, const char* mode, uint32_t base = 0,
                    uint32_t length = UINT32_MAX);
  bool resolve(const char* path, char* target, bool write);
  static void transactionPaths(uint8_t slot, char* data, char* backup, char* journal);
};
