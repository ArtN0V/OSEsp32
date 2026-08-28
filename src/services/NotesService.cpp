#include "NotesService.h"

#include <algorithm>
#include <strings.h>

namespace {
constexpr const char* NOTES_DIRECTORY = "/OSEsp32/Notes";
}

bool NotesService::isNotePath(const char* path) {
  if (!path) return false;
  const char* extension = strrchr(path, '.');
  return extension && !strcasecmp(extension, ".note");
}

void NotesService::parse(const char* text, char* title, size_t titleCapacity,
                         char* body, size_t bodyCapacity) {
  if (!title || !titleCapacity || !body || !bodyCapacity) return;
  title[0] = '\0';
  body[0] = '\0';
  if (!text) return;
  const char* lineEnd = strchr(text, '\n');
  const size_t titleLength = lineEnd ? static_cast<size_t>(lineEnd - text)
                                     : strlen(text);
  const size_t titleCopy = std::min(titleLength, titleCapacity - 1);
  memcpy(title, text, titleCopy);
  title[titleCopy] = '\0';
  if (lineEnd) strlcpy(body, lineEnd + 1, bodyCapacity);
}

uint8_t NotesService::list(NoteSummary* summaries, uint8_t capacity) {
  if (!storage_ || !storage_->mounted() || !summaries || !capacity) return 0;
  uint8_t resultCount = 0;
  uint16_t offset = 0;
  uint16_t total = 0;
  do {
    StorageEntry entries[StorageService::PAGE_ENTRIES];
    uint8_t count = 0;
    if (!storage_->listDirectoryPage(NOTES_DIRECTORY, offset, entries,
                                     StorageService::PAGE_ENTRIES, count,
                                     total))
      break;
    for (uint8_t index = 0; index < count && resultCount < capacity; ++index) {
      if (entries[index].directory || !isNotePath(entries[index].path)) continue;
      char prefix[322];
      size_t length = 0;
      if (!storage_->readFile(entries[index].path, prefix, sizeof(prefix),
                              length, true))
        continue;
      NoteSummary& summary = summaries[resultCount++];
      strlcpy(summary.path, entries[index].path, sizeof(summary.path));
      char body[193];
      parse(prefix, summary.title, sizeof(summary.title), body, sizeof(body));
      char* output = summary.preview;
      size_t outputLength = 0;
      for (const char* input = body;
           *input && outputLength + 1 < sizeof(summary.preview); ++input) {
        const char character = *input == '\n' || *input == '\r' ? ' ' : *input;
        if (character == ' ' && outputLength && output[outputLength - 1] == ' ')
          continue;
        output[outputLength++] = character;
      }
      output[outputLength] = '\0';
    }
    offset += count;
    if (!count) break;
  } while (offset < total && resultCount < capacity);
  return resultCount;
}

bool NotesService::load(const char* path, char* title, size_t titleCapacity,
                        char* body, size_t bodyCapacity) {
  if (!storage_ || !path || !title || !body) return false;
  const size_t capacity = titleCapacity + bodyCapacity + 1;
  char* document = new char[capacity];
  if (!document) return false;
  size_t length = 0;
  const bool loaded = storage_->readFile(path, document, capacity, length);
  if (loaded) parse(document, title, titleCapacity, body, bodyCapacity);
  delete[] document;
  return loaded;
}

bool NotesService::makeNewPath(char* path, size_t capacity) {
  if (!storage_ || !path || capacity < 32) return false;
  for (uint16_t attempt = 0; attempt < 1000; ++attempt) {
    const uint32_t id = millis() + nextId_++;
    snprintf(path, capacity, "%s/note_%08lx.note", NOTES_DIRECTORY,
             static_cast<unsigned long>(id));
    if (!storage_->exists(path)) return true;
  }
  return false;
}

bool NotesService::save(char* path, size_t pathCapacity, const char* title,
                        const char* body) {
  if (!storage_ || !storage_->mounted() || !path || !title || !body)
    return false;
  char newPath[129];
  if (!path[0]) {
    if (!makeNewPath(newPath, sizeof(newPath))) return false;
    if (strlcpy(path, newPath, pathCapacity) >= pathCapacity) return false;
  }
  const size_t titleLength = strlen(title);
  const size_t bodyLength = strlen(body);
  const size_t length = titleLength + 1 + bodyLength;
  char* document = new char[length + 1];
  if (!document) return false;
  memcpy(document, title, titleLength);
  document[titleLength] = '\n';
  memcpy(document + titleLength + 1, body, bodyLength);
  document[length] = '\0';
  const bool saved = storage_->writeFileAtomic(
      path, reinterpret_cast<const uint8_t*>(document), length);
  delete[] document;
  return saved;
}
