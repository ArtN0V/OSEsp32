#include "YapRuntimeService.h"

#include <esp_heap_caps.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "../vendor/lua549/lauxlib.h"
#include "../vendor/lua549/lua.h"
#include "../vendor/lua549/lualib.h"
}

namespace {
bool deadlineReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

void removeGlobal(lua_State* state, const char* name) {
  lua_pushnil(state);
  lua_setglobal(state, name);
}
}  // namespace

void* YapRuntimeService::allocate(void* userData, void* pointer,
                                  size_t oldSize, size_t newSize) {
  AllocationState* allocation = static_cast<AllocationState*>(userData);
  if (!allocation) return nullptr;
  if (!newSize) {
    free(pointer);
    if (pointer)
      allocation->current = oldSize <= allocation->current
                                ? allocation->current - oldSize
                                : 0;
    return nullptr;
  }
  const size_t accountedOld = pointer ? oldSize : 0;
  if (newSize > accountedOld &&
      (allocation->current > allocation->limit ||
       newSize - accountedOld > allocation->limit - allocation->current)) {
    allocation->rejected = true;
    return nullptr;
  }
  void* resized = realloc(pointer, newSize);
  if (!resized) {
    allocation->rejected = true;
    return nullptr;
  }
  allocation->current = allocation->current - accountedOld + newSize;
  if (allocation->current > allocation->peak)
    allocation->peak = allocation->current;
  return resized;
}

YapRuntimeService* YapRuntimeService::active(lua_State* state) {
  if (!state) return nullptr;
  return *static_cast<YapRuntimeService**>(lua_getextraspace(state));
}

const char* YapRuntimeService::readChunk(lua_State*, void* userData,
                                         size_t* size) {
  ReaderState* reader = static_cast<ReaderState*>(userData);
  *size = 0;
  if (!reader || reader->failed || !reader->remaining) return nullptr;
  const size_t wanted = min(static_cast<uint32_t>(sizeof(reader->bytes)),
                            reader->remaining);
  size_t received = 0;
  if (!reader->storage->readFileRange(reader->path, reader->offset,
                                      reader->bytes, wanted, received) ||
      received != wanted) {
    reader->failed = true;
    return nullptr;
  }
  reader->offset += received;
  reader->remaining -= received;
  *size = received;
  return reinterpret_cast<const char*>(reader->bytes);
}

void YapRuntimeService::instructionHook(lua_State* state, lua_Debug*) {
  YapRuntimeService* runtime = active(state);
  if (!runtime) return;
  runtime->hookInstructions_ += 1000;
  runtime->burstInstructions_ += 1000;
  // A yielded hook cannot be swallowed by Lua pcall. Policy is enforced by
  // the host before the coroutine is resumed again.
  if (lua_isyieldable(state)) lua_yield(state, 0);
  else if (runtime->burstInstructions_ >= INSTRUCTION_BUDGET)
    luaL_error(state, "non-yieldable execution limit exceeded");
}

int YapRuntimeService::setLabel(lua_State* state) {
  YapRuntimeService* runtime = active(state);
  if (!runtime || !runtime->activeResult_) return 0;
  size_t length = 0;
  const char* text = luaL_checklstring(state, 1, &length);
  if (length > sizeof(runtime->activeResult_->label) - 1)
    return luaL_error(state, "label is longer than 96 bytes");
  memcpy(runtime->activeResult_->label, text, length);
  runtime->activeResult_->label[length] = '\0';
  return 0;
}

int YapRuntimeService::initializeLibraries(lua_State* state) {
  luaL_requiref(state, LUA_GNAME, luaopen_base, 1);
  lua_pop(state, 1);
  luaL_requiref(state, LUA_TABLIBNAME, luaopen_table, 1);
  lua_pop(state, 1);
  luaL_requiref(state, LUA_STRLIBNAME, luaopen_string, 1);
  lua_pop(state, 1);
  luaL_requiref(state, LUA_MATHLIBNAME, luaopen_math, 1);
  lua_pop(state, 1);
  luaL_requiref(state, LUA_UTF8LIBNAME, luaopen_utf8, 1);
  lua_pop(state, 1);

  removeGlobal(state, "dofile");
  removeGlobal(state, "loadfile");
  removeGlobal(state, "load");
  removeGlobal(state, "print");
  removeGlobal(state, "warn");
  // Only the host creates/resumes coroutines. Prevent app-defined __gc and
  // __close callbacks from executing arbitrary code during forced teardown.
  removeGlobal(state, "setmetatable");
  removeGlobal(state, "getmetatable");
  // These C functions can call Lua without allowing a scheduling yield, or
  // spend unbounded time in pattern backtracking. Add bounded wrappers later.
  lua_getglobal(state, "table");
  lua_pushnil(state);
  lua_setfield(state, -2, "sort");
  lua_pop(state, 1);
  lua_getglobal(state, "string");
  const char* patterns[] = {"gsub", "gmatch", "find", "match"};
  for (const char* name : patterns) {
    lua_pushnil(state);
    lua_setfield(state, -2, name);
  }
  lua_pop(state, 1);

  lua_newtable(state);                  // osesp32
  lua_newtable(state);                  // osesp32.ui
  lua_pushcfunction(state, setLabel);
  lua_setfield(state, -2, "label");
  lua_setfield(state, -2, "ui");
  lua_pushcfunction(state, sleep);
  lua_setfield(state, -2, "sleep");
  lua_setglobal(state, "osesp32");
  return 0;
}

void YapRuntimeService::copyLuaError(lua_State* state, YapRuntimeResult& result,
                                     const char* fallback) {
  const char* message = state && lua_type(state, -1) == LUA_TSTRING
                            ? lua_tostring(state, -1) : nullptr;
  strlcpy(result.error, message ? message : fallback, sizeof(result.error));
}

int YapRuntimeService::sleep(lua_State* state) {
  YapRuntimeService* runtime = active(state);
  const lua_Integer duration = luaL_checkinteger(state, 1);
  if (duration < 1 || duration > 60000)
    return luaL_error(state, "sleep expects 1..60000 milliseconds");
  runtime->wakeMs_ = millis() + duration;
  runtime->sleeping_ = true;
  return lua_yield(state, 0);
}

int YapRuntimeService::prepare(lua_State* state) {
  YapRuntimeService* runtime = active(state);
  initializeLibraries(state);
  lua_pushstring(state, runtime->entry_);
  // The main state's stack roots the coroutine until lua_close.
  runtime->worker_ = lua_newthread(state);
  *static_cast<YapRuntimeService**>(lua_getextraspace(runtime->worker_)) = runtime;
  const YapPackageInfo& package = *runtime->loadingPackage_;
  const YapSection& code = package.sections[package.manifest.codeSection];
  ReaderState reader;
  reader.storage = runtime->storage_;
  reader.path = package.path;
  reader.offset = code.offset;
  reader.remaining = code.length;
  int status = lua_load(runtime->worker_, readChunk, &reader, package.path, "t");
  if (reader.failed || status != LUA_OK) {
    runtime->result_.status = reader.failed ? YapRuntimeStatus::IoError
        : status == LUA_ERRMEM ? YapRuntimeStatus::OutOfMemory
                               : YapRuntimeStatus::CompileError;
    copyLuaError(runtime->worker_, runtime->result_, "could not load source");
    return luaL_error(state, "source preparation failed");
  }
  return 2;
}

bool YapRuntimeService::start(const YapPackageInfo& package) {
  if (running_) return false;
  result_ = {};
  hookInstructions_ = burstInstructions_ = busyMs_ = 0;
  sleeping_ = entryStarted_ = false;
  allocation_ = {};
  startedMs_ = millis();
  result_.freeHeapBefore = ESP.getFreeHeap();
  result_.largestBlockBefore = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  result_.quotaBytes = package.manifest.requestedMemory;
  if (!storage_ || !logger_ || !storage_->mounted() ||
      package.sectionCount > YapPackageInfo::MAX_SECTIONS ||
      package.manifest.codeSection >= package.sectionCount ||
      result_.quotaBytes < YapPackageService::MIN_MEMORY ||
      result_.quotaBytes > YapPackageService::MAX_MEMORY) {
    finish(YapRuntimeStatus::BadPackage);
    return false;
  }
  const YapSection& code = package.sections[package.manifest.codeSection];
  if (code.type != YapPackageService::TYPE_LUA_SOURCE ||
      !code.length || code.length > MAX_SOURCE_SIZE) {
    finish(YapRuntimeStatus::BadPackage);
    return false;
  }
  strlcpy(entry_, package.manifest.entryPoint, sizeof(entry_));
  allocation_.limit = result_.quotaBytes;
  activeResult_ = &result_;
  state_ = lua_newstate(allocate, &allocation_);
  if (!state_) {
    finish(YapRuntimeStatus::OutOfMemory);
    return false;
  }
  *static_cast<YapRuntimeService**>(lua_getextraspace(state_)) = this;
  loadingPackage_ = &package;
  result_.status = YapRuntimeStatus::Success;
  lua_pushcfunction(state_, prepare);
  int status = lua_pcall(state_, 0, 2, 0);
  loadingPackage_ = nullptr;
  if (status != LUA_OK) {
    if (result_.status == YapRuntimeStatus::Success) {
      copyLuaError(state_, result_, "could not prepare VM");
      result_.status = status == LUA_ERRMEM ? YapRuntimeStatus::OutOfMemory
                                            : YapRuntimeStatus::ExecutionError;
    }
    finish(result_.status);
    return false;
  }
  lua_sethook(worker_, instructionHook, LUA_MASKCOUNT, 1000);
  running_ = true;
  return true;
}

void YapRuntimeService::update() {
  if (!running_) return;
  if (!storage_->mounted()) { finish(YapRuntimeStatus::IoError); return; }
  if (sleeping_) {
    if (!deadlineReached(millis(), wakeMs_)) return;
    sleeping_ = false;
    busyMs_ = burstInstructions_ = 0;
  }
  const uint32_t started = millis();
  int results = 0;
  const int status = lua_resume(worker_, state_, 0, &results);
  busyMs_ += millis() - started;
  result_.peakLuaBytes = allocation_.peak;
  if (burstInstructions_ >= INSTRUCTION_BUDGET || busyMs_ >= TIME_BUDGET_MS) {
    strlcpy(result_.error, "application execution limit exceeded",
            sizeof(result_.error));
    finish(YapRuntimeStatus::LimitExceeded);
  } else if (status == LUA_YIELD) {
    // Host hook or osesp32.sleep: leave the coroutine stack untouched.
  } else if (status != LUA_OK) {
    copyLuaError(worker_, result_, "application failed");
    finish(status == LUA_ERRMEM ? YapRuntimeStatus::OutOfMemory
                                 : YapRuntimeStatus::ExecutionError);
  } else if (!entryStarted_) {
    lua_settop(worker_, 0);
    // No user metatables: entry lookup cannot invoke a user callback.
    lua_pushglobaltable(worker_);
    lua_pushvalue(state_, 1);
    lua_xmove(state_, worker_, 1);
    lua_rawget(worker_, -2);
    lua_remove(worker_, -2);
    if (!lua_isfunction(worker_, -1)) {
      strlcpy(result_.error, "manifest entry function was not defined",
              sizeof(result_.error));
      finish(YapRuntimeStatus::EntryMissing);
    } else {
      entryStarted_ = true;
    }
  } else {
    finish(YapRuntimeStatus::Success);
  }
}

void YapRuntimeService::stop(YapRuntimeStatus reason) {
  if (running_ || state_) finish(reason);
}

void YapRuntimeService::finish(YapRuntimeStatus status) {
  if (state_) lua_close(state_);
  state_ = worker_ = nullptr;
  result_.status = status;
  result_.peakLuaBytes = allocation_.peak;
  result_.remainingLuaBytes = allocation_.current;
  result_.elapsedMs = millis() - startedMs_;
  result_.hookInstructions = hookInstructions_;
  result_.freeHeapAfter = ESP.getFreeHeap();
  result_.largestBlockAfter = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  activeResult_ = nullptr;
  running_ = false;
  if (logger_) logger_->info("yap-runtime", "status=%s peak=%lu remaining=%lu",
      statusCode(status), static_cast<unsigned long>(result_.peakLuaBytes),
      static_cast<unsigned long>(result_.remainingLuaBytes));
}

const char* YapRuntimeService::statusCode(YapRuntimeStatus status) {
  switch (status) {
    case YapRuntimeStatus::Success: return "ok";
    case YapRuntimeStatus::Busy: return "busy";
    case YapRuntimeStatus::BadPackage: return "bad_package";
    case YapRuntimeStatus::IoError: return "io_error";
    case YapRuntimeStatus::VmUnavailable: return "vm_unavailable";
    case YapRuntimeStatus::OutOfMemory: return "out_of_memory";
    case YapRuntimeStatus::CompileError: return "compile_error";
    case YapRuntimeStatus::EntryMissing: return "entry_missing";
    case YapRuntimeStatus::ExecutionError: return "execution_error";
    case YapRuntimeStatus::LimitExceeded: return "limit_exceeded";
    case YapRuntimeStatus::Cancelled: return "cancelled";
  }
  return "unknown";
}
