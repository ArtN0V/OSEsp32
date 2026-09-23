#pragma once

#include <Arduino.h>

#include "../kernel/Logger.h"
#include "../services/StorageService.h"
#include "../services/YapPackageService.h"
#include "../services/AppStorageService.h"

struct lua_State;
struct lua_Debug;

enum class YapRuntimeStatus : uint8_t {
  Success = 0,
  Busy,
  BadPackage,
  IoError,
  VmUnavailable,
  OutOfMemory,
  CompileError,
  EntryMissing,
  ExecutionError,
  LimitExceeded,
  Cancelled,
};

struct YapRuntimeResult {
  YapRuntimeStatus status = YapRuntimeStatus::BadPackage;
  bool exitedByApp = false;
  char label[97] = {};
  char error[129] = {};
  uint32_t quotaBytes = 0;
  uint32_t peakLuaBytes = 0;
  uint32_t remainingLuaBytes = 0;
  uint32_t freeHeapBefore = 0;
  uint32_t freeHeapAfter = 0;
  uint32_t largestBlockBefore = 0;
  uint32_t largestBlockAfter = 0;
  uint32_t elapsedMs = 0;
  uint32_t hookInstructions = 0;
};

class YapRuntimeService {
 public:
  enum class Request : uint8_t { None, Event, Text, Open, Save };
  struct Button { char text[49] = {}; };
  static constexpr uint32_t MAX_SOURCE_SIZE = 64u * 1024u;
  static constexpr uint32_t INSTRUCTION_BUDGET = 200000;
  static constexpr uint32_t TIME_BUDGET_MS = 250;

  void begin(StorageService& storage, Logger& logger) {
    storage_ = &storage;
    logger_ = &logger;
  }
  bool start(const YapPackageInfo& package);
  void update();
  void stop(YapRuntimeStatus reason = YapRuntimeStatus::Cancelled);
  bool running() const { return running_; }
  const YapRuntimeResult& result() const { return result_; }
  static const char* statusCode(YapRuntimeStatus status);
  Request request() const { return request_; }
  const char* requestText() const { return requestText_; }
  const Button* buttons() const { return buttons_; }
  uint32_t uiVersion() const { return uiVersion_; }
  void postEvent(uint8_t id);
  void reply(const char* text, int handle = 0, const char* error = nullptr);
  AppStorageService& files() { return files_; }
  void pauseStorage();
  void resumeStorage();
  bool grantInitialDocument(const char* path) { initialDocument_=files_.grant(path,"r"); return initialDocument_!=0; }

 private:
  struct AllocationState {
    size_t limit = 0;
    size_t current = 0;
    size_t peak = 0;
    bool rejected = false;
  };

  struct ReaderState {
    StorageService* storage = nullptr;
    const char* path = nullptr;
    uint32_t offset = 0;
    uint32_t remaining = 0;
    uint8_t bytes[256];
    bool failed = false;
  };

  StorageService* storage_ = nullptr;
  Logger* logger_ = nullptr;
  YapRuntimeResult* activeResult_ = nullptr;
  uint32_t hookInstructions_ = 0;
  bool running_ = false;
  lua_State* state_ = nullptr;
  lua_State* worker_ = nullptr;
  AllocationState allocation_;
  YapRuntimeResult result_;
  const YapPackageInfo* loadingPackage_ = nullptr;
  char entry_[25] = {};
  bool entryStarted_ = false;
  bool sleeping_ = false;
  bool appExitRequested_ = false;
  uint32_t wakeMs_ = 0;
  uint32_t startedMs_ = 0;
  uint32_t busyMs_ = 0;
  uint32_t burstInstructions_ = 0;
  AppStorageService files_;
  YapPackageInfo activePackage_;
  Button buttons_[6];
  uint8_t events_[8] = {}, eventHead_ = 0, eventCount_ = 0;
  uint32_t uiVersion_ = 0;
  Request request_ = Request::None;
  char requestText_[97] = {};
  char responseText_[193] = {};
  char responseError_[33] = {};
  int responseHandle_ = 0;
  bool responseReady_ = false, storagePaused_ = false;
  int initialDocument_=0;
  static int currentDocument(lua_State* state);
  static int fileCall(lua_State* state);
  static int uiButton(lua_State* state);
  static int waitEvent(lua_State* state);
  static int requestText(lua_State* state);
  static int requestOpen(lua_State* state);
  static int requestSave(lua_State* state);
  static int continueRequest(lua_State* state, int status, intptr_t context);
  static int makeRequest(lua_State* state, Request request, const char* text);
  static int prepare(lua_State* state);
  static int sleep(lua_State* state);
  static int exitApplication(lua_State* state);
  void finish(YapRuntimeStatus status);

  static void* allocate(void* userData, void* pointer, size_t oldSize,
                        size_t newSize);
  static const char* readChunk(lua_State* state, void* userData, size_t* size);
  static void instructionHook(lua_State* state, lua_Debug* debug);
  static int initializeLibraries(lua_State* state);
  static int setLabel(lua_State* state);
  static YapRuntimeService* active(lua_State* state);
  static void copyLuaError(lua_State* state, YapRuntimeResult& result,
                           const char* fallback);
};
