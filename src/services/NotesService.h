#pragma once

#include <Arduino.h>

#include "StorageService.h"

struct NoteSummary {
  char path[129] = {};
  char title[121] = {};
  char preview[129] = {};
};

class NotesService {
 public:
  static constexpr uint8_t MAX_NOTES = 24;
  static constexpr size_t TITLE_CAPACITY = 121;
  static constexpr size_t BODY_CAPACITY = 6145;
  static constexpr uint16_t MAX_TITLE_CHARACTERS = 40;
  static constexpr uint16_t MAX_BODY_CHARACTERS = 2048;

  void begin(StorageService& storage) { storage_ = &storage; }
  uint8_t list(NoteSummary* summaries, uint8_t capacity);
  bool load(const char* path, char* title, size_t titleCapacity, char* body,
            size_t bodyCapacity);
  bool save(char* path, size_t pathCapacity, const char* title,
            const char* body);

 private:
  StorageService* storage_ = nullptr;
  uint16_t nextId_ = 0;

  bool makeNewPath(char* path, size_t capacity);
  static bool isNotePath(const char* path);
  static void parse(const char* text, char* title, size_t titleCapacity,
                    char* body, size_t bodyCapacity);
};
