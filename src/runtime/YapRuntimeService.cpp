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
const char* checkedText(lua_State* state,int index,size_t maximum) {
  size_t length=0; const char* value=luaL_checklstring(state,index,&length);
  if (length>maximum || memchr(value,0,length)) luaL_error(state,"invalid text length or embedded NUL");
  for (size_t i=0;i<length;) {
    uint8_t c=static_cast<uint8_t>(value[i++]);
    if (c<128) continue;
    unsigned n; uint32_t cp,minimum;
    if ((c&0xe0)==0xc0) { n=1; cp=c&31; minimum=128; }
    else if ((c&0xf0)==0xe0) { n=2; cp=c&15; minimum=2048; }
    else if ((c&0xf8)==0xf0) { n=3; cp=c&7; minimum=65536; }
    else { luaL_error(state,"invalid UTF-8"); return nullptr; }
    if (n>length-i) luaL_error(state,"incomplete UTF-8");
    while (n--) {
      c=static_cast<uint8_t>(value[i++]);
      if ((c&0xc0)!=0x80) luaL_error(state,"invalid UTF-8");
      cp=(cp<<6)|(c&63);
    }
    if (cp<minimum || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) luaL_error(state,"invalid UTF-8");
  }
  return value;
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
  if (lua_type(state, 1) == LUA_TNUMBER) {
    UiWidget* item = widget(state, UiKind::Label, 1);
    const char* style = luaL_optstring(state, 7, "body");
    if (!strcmp(style, "body")) item->style = 0;
    else if (!strcmp(style, "title")) item->style = 1;
    else if (!strcmp(style, "status")) item->style = 2;
    else return luaL_error(state, "label style must be body, title or status");
    ++runtime->uiVersion_;
    return 0;
  }
  if (runtime->activePackage_.manifest.apiMinor>=2)
    return luaL_error(state,"API 1.2 label requires id,text,x,y,width,height");
  const char* text = checkedText(state,1,96);
  size_t length=strlen(text);
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
  lua_pushcfunction(state, uiButton); lua_setfield(state, -2, "button");
  lua_pushcfunction(state, waitEvent); lua_setfield(state, -2, "wait");
  lua_pushcfunction(state, requestText); lua_setfield(state, -2, "text");
  YapRuntimeService* runtime = active(state);
  if (runtime && runtime->activePackage_.manifest.apiMinor >= 2) {
    lua_pushcfunction(state, uiToggle); lua_setfield(state, -2, "toggle");
    lua_pushcfunction(state, uiTextField); lua_setfield(state, -2, "text_field");
    lua_pushcfunction(state, uiList); lua_setfield(state, -2, "list");
    lua_pushcfunction(state, uiClear); lua_setfield(state, -2, "clear");
    lua_pushcfunction(state, uiRemove); lua_setfield(state, -2, "remove");
    lua_pushcfunction(state, uiValue); lua_setfield(state, -2, "value");
    lua_pushcfunction(state, uiTimer); lua_setfield(state, -2, "timer");
    lua_pushcfunction(state, requestConfirm); lua_setfield(state, -2, "confirm");
  }
  lua_setfield(state, -2, "ui");
  if (runtime && runtime->activePackage_.manifest.apiMinor >= 3) {
    lua_newtable(state);
    lua_pushcfunction(state, requestCanvasProbe); lua_setfield(state, -2, "probe");
    lua_pushcfunction(state, requestCanvasRelease); lua_setfield(state, -2, "release");
    if (runtime->activePackage_.manifest.apiMinor >= 4) {
      lua_pushcfunction(state, requestCanvasCreate); lua_setfield(state, -2, "create");
      lua_pushcfunction(state, requestCanvasClear); lua_setfield(state, -2, "clear");
      lua_pushcfunction(state, requestCanvasLine); lua_setfield(state, -2, "line");
    }
    lua_setfield(state, -2, "canvas");
  }
  lua_newtable(state);
  const char* operations[] = {"open","read","write","seek","size","flush","close","stat","list","mkdir"};
  for (int i=0;i<10;++i) {
    lua_pushinteger(state,i); lua_pushcclosure(state,fileCall,1);
    lua_setfield(state,-2,operations[i]);
  }
  lua_setfield(state,-2,"fs");
  lua_newtable(state);
  lua_pushcfunction(state,requestOpen); lua_setfield(state,-2,"open");
  lua_pushcfunction(state,requestSave); lua_setfield(state,-2,"save");
  lua_pushcfunction(state,currentDocument); lua_setfield(state,-2,"current");
  lua_setfield(state,-2,"documents");
  lua_pushcfunction(state, sleep);
  lua_setfield(state, -2, "sleep");
  lua_pushcfunction(state, exitApplication);
  lua_setfield(state, -2, "exit");
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

int YapRuntimeService::exitApplication(lua_State* state) {
  if (!lua_isyieldable(state))
    return luaL_error(state, "exit requires a yieldable application context");
  active(state)->appExitRequested_ = true;
  // The host closes the VM after lua_resume returns, never from a Lua C call.
  // This coroutine is never resumed, even when exit was called inside pcall.
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
  appExitRequested_ = false;
  request_=Request::None; responseReady_=storagePaused_=false;
  canvasCommand_={};
  initialDocument_=0;
  eventHead_=eventCount_=0;
  for (auto& button:buttons_) button = {};
  for (auto& item:widgets_) item = {};
  for (auto& timer:timers_) timer = {};
  widgetCount_=listRows_=0;
  viewportWidth_=package.manifest.launchMode==YapLaunchMode::Windowed ? 300 : 320;
  viewportHeight_=package.manifest.launchMode==YapLaunchMode::Windowed ? 148 : 240;
  ++uiVersion_;
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
  activePackage_=package;
  files_.begin(*storage_,package);
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
  if (!storage_->mounted()) pauseStorage();
  if (storagePaused_) return;
  pumpTimers();
  int arguments=0;
  if (request_!=Request::None) {
    if (request_==Request::Event && eventCount_) {
      responseEvent_=events_[eventHead_]; eventHead_=(eventHead_+1)%MAX_UI_EVENTS;
      responseHandle_=responseEvent_.id;
      --eventCount_; responseError_[0]=0; responseReady_=true;
    }
    if (!responseReady_) return;
    request_=Request::None; responseReady_=false;
    busyMs_=burstInstructions_=0;
  }
  if (sleeping_) {
    if (!deadlineReached(millis(), wakeMs_)) return;
    sleeping_ = false;
    busyMs_ = burstInstructions_ = 0;
  }
  const uint32_t started = millis();
  int results = 0;
  const int status = lua_resume(worker_, state_, arguments, &results);
  busyMs_ += millis() - started;
  result_.peakLuaBytes = allocation_.peak;
  if (appExitRequested_) {
    result_.exitedByApp = true;
    finish(YapRuntimeStatus::Success);
  } else if (burstInstructions_ >= INSTRUCTION_BUDGET || busyMs_ >= TIME_BUDGET_MS) {
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
  files_.end();
  request_=Request::None;
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

int YapRuntimeService::fileCall(lua_State* state) {
  auto& fs=active(state)->files_;
  const int op=lua_tointeger(state,lua_upvalueindex(1));
  bool ok=false; uint32_t size=0;
  if (op==0) {
    int token=fs.open(checkedText(state,1,127),checkedText(state,2,1));
    if (token) { lua_pushinteger(state,token); return 1; }
  } else if (op==7) {
    ok=fs.stat(checkedText(state,1,127),size);
    if (ok && size<=INT32_MAX) { lua_pushinteger(state,size); return 1; }
  } else if (op==8) {
    const char* path=checkedText(state,1,127);
    lua_Integer page=luaL_optinteger(state,2,0);
    if (page<0 || page>16383) return luaL_error(state,"invalid page");
    StorageEntry entries[4]; uint8_t count=0;
    if (fs.list(path,page,entries,count)) {
      lua_newtable(state);
      for (uint8_t i=0;i<count;++i) {
        lua_newtable(state);
        lua_pushstring(state,entries[i].name); lua_setfield(state,-2,"name");
        lua_pushboolean(state,entries[i].directory); lua_setfield(state,-2,"directory");
        lua_pushnumber(state,entries[i].size); lua_setfield(state,-2,"size");
        lua_rawseti(state,-2,i+1);
      }
      return 1;
    }
  } else if (op==9) ok=fs.mkdir(checkedText(state,1,127));
  else {
    int token=luaL_checkinteger(state,1);
    if (op==1) {
      lua_Integer wanted=luaL_checkinteger(state,2);
      if (wanted<0 || wanted>512) return luaL_error(state,"read expects 0..512 bytes");
      uint8_t data[512]; size_t count=0;
      if (fs.read(token,data,wanted,count)) { lua_pushlstring(state,reinterpret_cast<char*>(data),count); return 1; }
    } else if (op==2) {
      size_t count; const char* bytes=luaL_checklstring(state,2,&count);
      ok=fs.write(token,reinterpret_cast<const uint8_t*>(bytes),count);
    } else if (op==3) {
      lua_Integer offset=luaL_checkinteger(state,2);
      if (offset<0) return luaL_error(state,"negative offset");
      ok=fs.seek(token,offset);
    } else if (op==4) {
      if (fs.size(token,size) && size<=INT32_MAX) { lua_pushinteger(state,size); return 1; }
    } else if (op==5) ok=fs.flush(token);
    else if (op==6) ok=fs.close(token,lua_isnoneornil(state,2) || lua_toboolean(state,2));
  }
  if (ok) { lua_pushboolean(state,true); return 1; }
  lua_pushnil(state); lua_pushstring(state,fs.error()); return 2;
}
YapRuntimeService::UiWidget* YapRuntimeService::findWidget(
    YapRuntimeService* runtime, uint8_t id) {
  if (!runtime || !id) return nullptr;
  for (uint8_t index=0; index<runtime->widgetCount_; ++index)
    if (runtime->widgets_[index].id==id) return &runtime->widgets_[index];
  return nullptr;
}

YapRuntimeService::UiWidget* YapRuntimeService::widget(
    lua_State* state, UiKind kind, int argumentOffset) {
  auto* runtime=active(state);
  if (!runtime || runtime->activePackage_.manifest.apiMinor<2)
    luaL_error(state,"rich UI requires API 1.2");
  const lua_Integer rawId=luaL_checkinteger(state,argumentOffset);
  if (rawId<1 || rawId>255) luaL_error(state,"widget id must be 1..255");
  const char* text=checkedText(state,argumentOffset+1,96);
  const lua_Integer x=luaL_checkinteger(state,argumentOffset+2);
  const lua_Integer y=luaL_checkinteger(state,argumentOffset+3);
  const lua_Integer width=luaL_checkinteger(state,argumentOffset+4);
  const lua_Integer height=luaL_checkinteger(state,argumentOffset+5);
  if (x<0 || y<0 || width<8 || height<8 || x>runtime->viewportWidth_ ||
      y>runtime->viewportHeight_ || width>runtime->viewportWidth_-x ||
      height>runtime->viewportHeight_-y)
    luaL_error(state,"widget geometry is outside the application viewport");
  UiWidget* item=findWidget(runtime,static_cast<uint8_t>(rawId));
  if (!item) {
    if (runtime->widgetCount_==MAX_UI_WIDGETS)
      luaL_error(state,"too many UI widgets");
    item=&runtime->widgets_[runtime->widgetCount_++];
  } else if (item->kind==UiKind::List) {
    runtime->listRows_-=item->rowCount;
  }
  *item={}; item->id=static_cast<uint8_t>(rawId); item->kind=kind;
  item->x=x; item->y=y; item->width=width; item->height=height;
  strlcpy(item->text,text,sizeof(item->text));
  return item;
}

int YapRuntimeService::uiButton(lua_State* state) {
  auto* runtime=active(state);
  if (lua_gettop(state)==2) {
    if (runtime->activePackage_.manifest.apiMinor>=2)
      return luaL_error(state,"API 1.2 button requires id,text,x,y,width,height");
    int id=luaL_checkinteger(state,1);
    if (id<1 || id>6) return luaL_error(state,"button id must be 1..6");
    strlcpy(runtime->buttons_[id-1].text,checkedText(state,2,48),49);
  } else {
    widget(state,UiKind::Button,1);
  }
  ++runtime->uiVersion_; return 0;
}

int YapRuntimeService::uiToggle(lua_State* state) {
  auto* runtime=active(state);
  if (!runtime || runtime->activePackage_.manifest.apiMinor<2)
    return luaL_error(state,"rich UI requires API 1.2");
  const lua_Integer id=luaL_checkinteger(state,1);
  if (id<1 || id>255) return luaL_error(state,"widget id must be 1..255");
  const char* text=checkedText(state,2,96);
  const bool checked=lua_toboolean(state,3);
  const lua_Integer x=luaL_checkinteger(state,4), y=luaL_checkinteger(state,5);
  const lua_Integer width=luaL_checkinteger(state,6), height=luaL_checkinteger(state,7);
  if (x<0 || y<0 || width<8 || height<8 || x>runtime->viewportWidth_ ||
      y>runtime->viewportHeight_ || width>runtime->viewportWidth_-x ||
      height>runtime->viewportHeight_-y)
    return luaL_error(state,"widget geometry is outside the application viewport");
  UiWidget* item=findWidget(runtime,static_cast<uint8_t>(id));
  if (!item) {
    if (runtime->widgetCount_==MAX_UI_WIDGETS) return luaL_error(state,"too many UI widgets");
    item=&runtime->widgets_[runtime->widgetCount_++];
  } else if (item->kind==UiKind::List) runtime->listRows_-=item->rowCount;
  *item={}; item->id=static_cast<uint8_t>(id); item->kind=UiKind::Toggle;
  item->x=x; item->y=y; item->width=width; item->height=height;
  item->checked=checked; strlcpy(item->text,text,sizeof(item->text));
  ++runtime->uiVersion_; return 0;
}

int YapRuntimeService::uiTextField(lua_State* state) {
  auto* runtime=active(state); widget(state,UiKind::TextField,1);
  ++runtime->uiVersion_; return 0;
}

int YapRuntimeService::uiList(lua_State* state) {
  auto* runtime=active(state);
  if (!runtime || runtime->activePackage_.manifest.apiMinor<2)
    return luaL_error(state,"rich UI requires API 1.2");
  const lua_Integer rawId=luaL_checkinteger(state,1);
  if (rawId<1 || rawId>255) return luaL_error(state,"widget id must be 1..255");
  luaL_checktype(state,2,LUA_TTABLE);
  const size_t count=lua_rawlen(state,2);
  if (count>UiWidget::MAX_ROWS) return luaL_error(state,"list has more than six rows");
  const lua_Integer x=luaL_checkinteger(state,3), y=luaL_checkinteger(state,4);
  const lua_Integer width=luaL_checkinteger(state,5), height=luaL_checkinteger(state,6);
  if (x<0 || y<0 || width<32 || height<24 || x>runtime->viewportWidth_ ||
      y>runtime->viewportHeight_ || width>runtime->viewportWidth_-x ||
      height>runtime->viewportHeight_-y)
    return luaL_error(state,"widget geometry is outside the application viewport");
  UiWidget* item=findWidget(runtime,static_cast<uint8_t>(rawId));
  const uint8_t previous=item && item->kind==UiKind::List ? item->rowCount : 0;
  if (runtime->listRows_-previous+count>MAX_UI_LIST_ROWS)
    return luaL_error(state,"too many UI list rows");
  if (!item) {
    if (runtime->widgetCount_==MAX_UI_WIDGETS) return luaL_error(state,"too many UI widgets");
    item=&runtime->widgets_[runtime->widgetCount_++];
  }
  *item={}; item->id=rawId; item->kind=UiKind::List;
  item->x=x; item->y=y; item->width=width; item->height=height;
  item->rowCount=count; runtime->listRows_=runtime->listRows_-previous+count;
  for (size_t index=0;index<count;++index) {
    lua_rawgeti(state,2,index+1);
    strlcpy(item->rows[index],checkedText(state,-1,32),sizeof(item->rows[index]));
    lua_pop(state,1);
  }
  ++runtime->uiVersion_; return 0;
}

int YapRuntimeService::uiClear(lua_State* state) {
  auto* runtime=active(state);
  if (runtime->activePackage_.manifest.apiMinor<2) return luaL_error(state,"rich UI requires API 1.2");
  for (auto& item:runtime->widgets_) item={};
  for (auto& timer:runtime->timers_) timer={};
  runtime->widgetCount_=runtime->listRows_=0;
  runtime->eventHead_=runtime->eventCount_=0;
  ++runtime->uiVersion_; return 0;
}

int YapRuntimeService::uiRemove(lua_State* state) {
  auto* runtime=active(state); const lua_Integer id=luaL_checkinteger(state,1);
  if (id<1 || id>255) return luaL_error(state,"widget id must be 1..255");
  for (uint8_t index=0;index<runtime->widgetCount_;++index) if (runtime->widgets_[index].id==id) {
    if (runtime->widgets_[index].kind==UiKind::List) runtime->listRows_-=runtime->widgets_[index].rowCount;
    for (uint8_t move=index+1;move<runtime->widgetCount_;++move)
      runtime->widgets_[move-1]=runtime->widgets_[move];
    runtime->widgets_[--runtime->widgetCount_]={}; ++runtime->uiVersion_;
    lua_pushboolean(state,true); return 1;
  }
  lua_pushboolean(state,false); return 1;
}

int YapRuntimeService::uiValue(lua_State* state) {
  const lua_Integer id=luaL_checkinteger(state,1);
  if (id<1 || id>255) return luaL_error(state,"widget id must be 1..255");
  auto* item=findWidget(active(state),static_cast<uint8_t>(id));
  if (!item) { lua_pushnil(state); lua_pushliteral(state,"not_found"); return 2; }
  if (item->kind==UiKind::Toggle) lua_pushboolean(state,item->checked);
  else if (item->kind==UiKind::List) lua_pushinteger(state,item->selected);
  else lua_pushstring(state,item->text);
  return 1;
}

int YapRuntimeService::uiTimer(lua_State* state) {
  auto* runtime=active(state); const lua_Integer id=luaL_checkinteger(state,1);
  const lua_Integer interval=luaL_checkinteger(state,2); const bool repeat=lua_toboolean(state,3);
  if (id<1 || id>255) return luaL_error(state,"timer id must be 1..255");
  UiTimer* available=nullptr;
  for (auto& timer:runtime->timers_) {
    if (timer.id==id) { available=&timer; break; }
    if (!timer.id && !available) available=&timer;
  }
  if (!interval) {
    if (available && available->id==id) *available={};
    return 0;
  }
  if (interval<50 || interval>60000) return luaL_error(state,"timer interval must be 50..60000 ms or zero");
  if (!available) return luaL_error(state,"too many UI timers");
  available->id=id; available->interval=interval; available->repeat=repeat;
  available->due=millis()+interval; return 0;
}

void YapRuntimeService::postEvent(uint8_t id) {
  if (!running_ || storagePaused_ || id<1 || id>6 || !*buttons_[id-1].text) return;
  postUiEvent(id,UiEventKind::Tap);
}

void YapRuntimeService::postUiEvent(uint8_t id, UiEventKind kind, int16_t value,
                                    const char* text) {
  if (!running_ || storagePaused_ || !id || eventCount_==MAX_UI_EVENTS) return;
  UiWidget* item=findWidget(this,id);
  if (kind!=UiEventKind::Timer && richUi() && !item) return;
  if (item && kind==UiEventKind::Change) {
    if (item->kind==UiKind::Toggle) item->checked=value!=0;
    if (item->kind==UiKind::List) { item->selected=value; ++uiVersion_; }
    if (item->kind==UiKind::TextField && text) {
      strlcpy(item->text,text,sizeof(item->text)); ++uiVersion_;
    }
  }
  UiEvent& queued=events_[(eventHead_+eventCount_)%MAX_UI_EVENTS];
  queued={}; queued.id=id; queued.kind=kind; queued.value=value;
  if (text) strlcpy(queued.text,text,sizeof(queued.text));
  ++eventCount_;
}

void YapRuntimeService::postCanvasEvent(UiEventKind kind, int16_t x, int16_t y) {
  if (!running_ || storagePaused_ || activePackage_.manifest.apiMinor<4 ||
      eventCount_==MAX_UI_EVENTS ||
      (kind!=UiEventKind::CanvasDown && kind!=UiEventKind::CanvasMove &&
       kind!=UiEventKind::CanvasUp)) return;
  UiEvent& queued=events_[(eventHead_+eventCount_)%MAX_UI_EVENTS];
  queued={}; queued.kind=kind; queued.value=x; queued.secondary=y;
  ++eventCount_;
}

bool YapRuntimeService::setWidgetText(uint8_t id,const char* text) {
  UiWidget* item=findWidget(this,id);
  if (!item || item->kind!=UiKind::TextField || !text || strlen(text)>96) return false;
  strlcpy(item->text,text,sizeof(item->text)); ++uiVersion_; return true;
}

const char* YapRuntimeService::eventName(UiEventKind kind) {
  switch (kind) {
    case UiEventKind::Tap: return "tap";
    case UiEventKind::Change: return "change";
    case UiEventKind::Hold: return "hold";
    case UiEventKind::SwipeLeft: return "swipe_left";
    case UiEventKind::SwipeRight: return "swipe_right";
    case UiEventKind::SwipeUp: return "swipe_up";
    case UiEventKind::SwipeDown: return "swipe_down";
    case UiEventKind::Timer: return "timer";
    case UiEventKind::CanvasDown: return "canvas_down";
    case UiEventKind::CanvasMove: return "canvas_move";
    case UiEventKind::CanvasUp: return "canvas_up";
  }
  return "tap";
}

void YapRuntimeService::pumpTimers() {
  const uint32_t now=millis();
  for (auto& timer:timers_) if (timer.id && deadlineReached(now,timer.due)) {
    postUiEvent(timer.id,UiEventKind::Timer);
    if (timer.repeat) timer.due=now+timer.interval;
    else timer={};
  }
}
int YapRuntimeService::continueRequest(lua_State* state,int,intptr_t context) {
  auto* runtime=active(state);
  // Allocate response strings only inside lua_resume's protected boundary.
  if (*runtime->responseError_) {
    lua_pushnil(state); lua_pushstring(state,runtime->responseError_); return 2;
  }
  const Request request=static_cast<Request>(context);
  if (request==Request::Text) { lua_pushstring(state,runtime->responseText_); return 1; }
  if (request==Request::Confirm) { lua_pushboolean(state,runtime->responseHandle_!=0); return 1; }
  if (request==Request::CanvasProbe || request==Request::CanvasRelease) {
    const CanvasStats& stats=runtime->responseCanvas_;
    lua_newtable(state);
    lua_pushstring(state,stats.format); lua_setfield(state,-2,"format");
    lua_pushinteger(state,stats.bufferBytes); lua_setfield(state,-2,"buffer_bytes");
    lua_pushinteger(state,stats.freeBefore); lua_setfield(state,-2,"free_before");
    lua_pushinteger(state,stats.freeActive); lua_setfield(state,-2,"free_active");
    lua_pushinteger(state,stats.largestBefore); lua_setfield(state,-2,"largest_before");
    lua_pushinteger(state,stats.largestActive); lua_setfield(state,-2,"largest_active");
    lua_pushinteger(state,stats.minimumFree); lua_setfield(state,-2,"minimum_free");
    lua_pushinteger(state,stats.allocationUs); lua_setfield(state,-2,"allocation_us");
    lua_pushinteger(state,stats.fillUs); lua_setfield(state,-2,"fill_us");
    lua_pushinteger(state,stats.frameCount); lua_setfield(state,-2,"frame_count");
    lua_pushinteger(state,stats.averageFrameMs); lua_setfield(state,-2,"average_frame_ms");
    lua_pushinteger(state,stats.maximumFrameMs); lua_setfield(state,-2,"maximum_frame_ms");
    return 1;
  }
  if (request==Request::Event) {
    lua_pushinteger(state,runtime->responseEvent_.id);
    lua_pushstring(state,eventName(runtime->responseEvent_.kind));
    if (runtime->responseEvent_.kind==UiEventKind::CanvasDown ||
        runtime->responseEvent_.kind==UiEventKind::CanvasMove ||
        runtime->responseEvent_.kind==UiEventKind::CanvasUp) {
      lua_pushinteger(state,runtime->responseEvent_.value);
      lua_pushinteger(state,runtime->responseEvent_.secondary);
      return 4;
    }
    UiWidget* item=findWidget(runtime,runtime->responseEvent_.id);
    if (runtime->responseEvent_.text[0]) lua_pushstring(state,runtime->responseEvent_.text);
    else if (runtime->responseEvent_.kind==UiEventKind::Change && item && item->kind==UiKind::Toggle)
      lua_pushboolean(state,runtime->responseEvent_.value!=0);
    else if (runtime->responseEvent_.kind==UiEventKind::Change)
      lua_pushinteger(state,runtime->responseEvent_.value);
    else lua_pushnil(state);
    return 3;
  }
  if (request==Request::CanvasCreate || request==Request::CanvasClear ||
      request==Request::CanvasLine) {
    lua_pushboolean(state,1); return 1;
  }
  lua_pushinteger(state,runtime->responseHandle_); return 1;
}
int YapRuntimeService::makeRequest(lua_State* state,Request request,const char* text) {
  auto* runtime=active(state);
  if (!lua_isyieldable(state)) return luaL_error(state,"request requires yieldable context");
  runtime->request_=request; runtime->responseReady_=false;
  strlcpy(runtime->requestText_,text ? text : "",sizeof(runtime->requestText_));
  lua_settop(state,0);
  return lua_yieldk(state,0,static_cast<intptr_t>(request),continueRequest);
}
int YapRuntimeService::waitEvent(lua_State* state) { return makeRequest(state,Request::Event,nullptr); }
int YapRuntimeService::requestText(lua_State* state) { return makeRequest(state,Request::Text,checkedText(state,1,96)); }
int YapRuntimeService::requestConfirm(lua_State* state) {
  return makeRequest(state,Request::Confirm,checkedText(state,1,96));
}
int YapRuntimeService::requestCanvasProbe(lua_State* state) {
  auto* runtime=active(state);
  if (runtime->activePackage_.manifest.launchMode!=YapLaunchMode::Exclusive) {
    lua_pushnil(state); lua_pushliteral(state,"exclusive_required"); return 2;
  }
  const char* format=checkedText(state,1,7);
  if (strcmp(format,"rgb565") && strcmp(format,"i8") && strcmp(format,"i4"))
    return luaL_error(state,"canvas format must be rgb565, i8 or i4");
  return makeRequest(state,Request::CanvasProbe,format);
}
int YapRuntimeService::requestCanvasRelease(lua_State* state) {
  return makeRequest(state,Request::CanvasRelease,nullptr);
}
int YapRuntimeService::requestCanvasCreate(lua_State* state) {
  auto* runtime=active(state);
  if (runtime->activePackage_.manifest.launchMode!=YapLaunchMode::Exclusive) {
    lua_pushnil(state); lua_pushliteral(state,"exclusive_required"); return 2;
  }
  const lua_Integer width=luaL_checkinteger(state,1);
  const lua_Integer height=luaL_checkinteger(state,2);
  if (width<16 || width>320 || height<16 || height>204)
    return luaL_error(state,"canvas size must be 16..320 by 16..204");
  runtime->canvasCommand_={};
  runtime->canvasCommand_.width=width;
  runtime->canvasCommand_.height=height;
  return makeRequest(state,Request::CanvasCreate,nullptr);
}
int YapRuntimeService::requestCanvasClear(lua_State* state) {
  auto* runtime=active(state);
  const lua_Integer color=luaL_checkinteger(state,1);
  if (color<0 || color>15) return luaL_error(state,"canvas color must be 0..15");
  runtime->canvasCommand_={}; runtime->canvasCommand_.color=color;
  return makeRequest(state,Request::CanvasClear,nullptr);
}
int YapRuntimeService::requestCanvasLine(lua_State* state) {
  auto* runtime=active(state);
  const lua_Integer x1=luaL_checkinteger(state,1), y1=luaL_checkinteger(state,2);
  const lua_Integer x2=luaL_checkinteger(state,3), y2=luaL_checkinteger(state,4);
  const lua_Integer color=luaL_checkinteger(state,5), thickness=luaL_checkinteger(state,6);
  if (x1<0 || x1>319 || x2<0 || x2>319 || y1<0 || y1>203 || y2<0 || y2>203)
    return luaL_error(state,"canvas line coordinates are out of range");
  if (color<0 || color>15 || thickness<1 || thickness>9)
    return luaL_error(state,"canvas line expects color 0..15 and thickness 1..9");
  runtime->canvasCommand_={};
  runtime->canvasCommand_.x1=x1; runtime->canvasCommand_.y1=y1;
  runtime->canvasCommand_.x2=x2; runtime->canvasCommand_.y2=y2;
  runtime->canvasCommand_.color=color; runtime->canvasCommand_.thickness=thickness;
  return makeRequest(state,Request::CanvasLine,nullptr);
}
int YapRuntimeService::requestOpen(lua_State* state) {
  if (!(active(state)->activePackage_.manifest.capabilities&YapDocumentsOpen)) {
    lua_pushnil(state); lua_pushliteral(state,"permission_denied"); return 2;
  }
  return makeRequest(state,Request::Open,nullptr);
}
int YapRuntimeService::requestSave(lua_State* state) {
  if (!(active(state)->activePackage_.manifest.capabilities&(YapDocumentsCreate|YapDocumentsReplace))) {
    lua_pushnil(state); lua_pushliteral(state,"permission_denied"); return 2;
  }
  const char* name=checkedText(state,1,48);
  if (!AppStorageService::validRelative(name) || strchr(name,'/')) return luaL_error(state,"invalid filename");
  return makeRequest(state,Request::Save,name);
}
void YapRuntimeService::reply(const char* text,int handle,const char* error) {
  if (request_==Request::None) return;
  strlcpy(responseText_,text ? text : "",sizeof(responseText_));
  strlcpy(responseError_,error ? error : "",sizeof(responseError_));
  responseHandle_=handle; responseReady_=true;
}
void YapRuntimeService::replyCanvas(const CanvasStats& stats,const char* error) {
  if (request_!=Request::CanvasProbe && request_!=Request::CanvasRelease) return;
  responseCanvas_=stats;
  strlcpy(responseError_,error ? error : "",sizeof(responseError_));
  responseReady_=true;
}
void YapRuntimeService::replyCanvasCommand(const char* error) {
  if (request_!=Request::CanvasCreate && request_!=Request::CanvasClear &&
      request_!=Request::CanvasLine) return;
  strlcpy(responseError_,error ? error : "",sizeof(responseError_));
  responseReady_=true;
}
void YapRuntimeService::pauseStorage() {
  if (storagePaused_) return;
  storagePaused_=true; files_.invalidate(); eventHead_=eventCount_=0;
  initialDocument_=0;
  if (request_!=Request::None && request_!=Request::Event) reply(nullptr,0,"storage_removed");
}
int YapRuntimeService::currentDocument(lua_State* state) {
  auto* runtime=active(state);
  if (runtime->initialDocument_) { lua_pushinteger(state,runtime->initialDocument_); runtime->initialDocument_=0; return 1; }
  lua_pushnil(state); lua_pushliteral(state,"no_document"); return 2;
}
void YapRuntimeService::resumeStorage() {
  files_.begin(*storage_,activePackage_); storagePaused_=false;
  busyMs_=burstInstructions_=0;
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
