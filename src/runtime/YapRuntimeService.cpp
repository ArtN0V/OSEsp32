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
  if (runtime->hookInstructions_ >= INSTRUCTION_BUDGET ||
      deadlineReached(millis(), runtime->deadlineMs_)) {
    runtime->limitExceeded_ = true;
    luaL_error(state, "application execution limit exceeded");
  }
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
  luaL_requiref(state, LUA_COLIBNAME, luaopen_coroutine, 1);
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

  lua_newtable(state);                  // osesp32
  lua_newtable(state);                  // osesp32.ui
  lua_pushcfunction(state, setLabel);
  lua_setfield(state, -2, "label");
  lua_setfield(state, -2, "ui");
  lua_setglobal(state, "osesp32");
  return 0;
}

void YapRuntimeService::copyLuaError(lua_State* state, YapRuntimeResult& result,
                                     const char* fallback) {
  const char* message = state ? lua_tostring(state, -1) : nullptr;
  strlcpy(result.error, message ? message : fallback, sizeof(result.error));
}

YapRuntimeStatus YapRuntimeService::run(const YapPackageInfo& package,
                                        YapRuntimeResult& result) {
  result = {};
  result.status = YapRuntimeStatus::BadPackage;
  if (running_) return result.status = YapRuntimeStatus::Busy;
  if (!storage_ || !logger_ || !storage_->mounted() ||
      package.manifest.codeSection >= package.sectionCount)
    return result.status = YapRuntimeStatus::BadPackage;
  const YapSection& code = package.sections[package.manifest.codeSection];
  if (code.type != YapPackageService::TYPE_LUA_SOURCE || !code.length ||
      code.length > MAX_SOURCE_SIZE)
    return result.status = YapRuntimeStatus::BadPackage;

  running_ = true;
  activeResult_ = &result;
  result.quotaBytes = package.manifest.requestedMemory;
  result.freeHeapBefore = ESP.getFreeHeap();
  result.largestBlockBefore =
      heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  const uint32_t started = millis();
  AllocationState allocation;
  allocation.limit = package.manifest.requestedMemory;
  lua_State* state = lua_newstate(allocate, &allocation);
  if (!state) {
    result.status = allocation.rejected ? YapRuntimeStatus::OutOfMemory
                                        : YapRuntimeStatus::VmUnavailable;
    strlcpy(result.error, "could not create Lua state", sizeof(result.error));
  } else {
    *static_cast<YapRuntimeService**>(lua_getextraspace(state)) = this;
    lua_pushcfunction(state, initializeLibraries);
    int luaStatus = lua_pcall(state, 0, 0, 0);
    if (luaStatus != LUA_OK) {
      result.status = allocation.rejected ? YapRuntimeStatus::OutOfMemory
                                          : YapRuntimeStatus::ExecutionError;
      copyLuaError(state, result, "could not initialize safe libraries");
    } else {
      ReaderState reader;
      reader.storage = storage_;
      reader.path = package.path;
      reader.offset = code.offset;
      reader.remaining = code.length;
      luaStatus = lua_load(state, readChunk, &reader, package.path, "t");
      if (reader.failed) {
        result.status = YapRuntimeStatus::IoError;
        strlcpy(result.error, "could not stream LUAS section",
                sizeof(result.error));
      } else if (luaStatus != LUA_OK) {
        result.status = allocation.rejected ? YapRuntimeStatus::OutOfMemory
                                            : YapRuntimeStatus::CompileError;
        copyLuaError(state, result, "Lua source did not compile");
      } else {
        hookInstructions_ = 0;
        limitExceeded_ = false;
        deadlineMs_ = millis() + TIME_BUDGET_MS;
        lua_sethook(state, instructionHook, LUA_MASKCOUNT, 1000);
        luaStatus = lua_pcall(state, 0, 0, 0);
        if (luaStatus == LUA_OK) {
          lua_getglobal(state, package.manifest.entryPoint);
          if (!lua_isfunction(state, -1)) {
            lua_pop(state, 1);
            result.status = YapRuntimeStatus::EntryMissing;
            strlcpy(result.error, "manifest entry function was not defined",
                    sizeof(result.error));
          } else {
            luaStatus = lua_pcall(state, 0, 0, 0);
            if (luaStatus == LUA_OK) {
              result.status = YapRuntimeStatus::Success;
            } else {
              result.status = limitExceeded_
                                  ? YapRuntimeStatus::LimitExceeded
                                  : allocation.rejected
                                        ? YapRuntimeStatus::OutOfMemory
                                        : YapRuntimeStatus::ExecutionError;
              copyLuaError(state, result, "entry function failed");
            }
          }
        } else {
          result.status = limitExceeded_ ? YapRuntimeStatus::LimitExceeded
                                         : allocation.rejected
                                               ? YapRuntimeStatus::OutOfMemory
                                               : YapRuntimeStatus::ExecutionError;
          copyLuaError(state, result, "top-level chunk failed");
        }
        lua_sethook(state, nullptr, 0, 0);
      }
    }
    lua_close(state);
  }
  result.peakLuaBytes = allocation.peak;
  result.remainingLuaBytes = allocation.current;
  result.elapsedMs = millis() - started;
  result.hookInstructions = hookInstructions_;
  result.freeHeapAfter = ESP.getFreeHeap();
  result.largestBlockAfter =
      heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  logger_->info("yap-runtime",
                "status=%s quota=%lu peak=%lu remaining=%lu elapsed=%lu",
                statusCode(result.status),
                static_cast<unsigned long>(result.quotaBytes),
                static_cast<unsigned long>(result.peakLuaBytes),
                static_cast<unsigned long>(result.remainingLuaBytes),
                static_cast<unsigned long>(result.elapsedMs));
  activeResult_ = nullptr;
  running_ = false;
  return result.status;
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
  }
  return "unknown";
}
