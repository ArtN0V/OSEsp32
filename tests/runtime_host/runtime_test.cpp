#include "../../src/runtime/YapRuntimeService.h"
#include "../../src/runtime/AppLifecycle.h"
#include <cassert>
#include <cstdio>
#include <string>

uint32_t testMillis = 0;
TestESP ESP;
static std::string source;
static bool simulateRemoved = false;
StorageService::StorageService() { mounted_ = true; }
void StorageService::update() { mounted_ = !simulateRemoved; }
void Logger::info(const char*, const char*, ...) {}
bool StorageService::readFileRange(const char*, uint32_t offset, uint8_t* out,
                                  size_t length, size_t& received) const {
  if (offset > source.size()) return false;
  received = std::min(length, source.size() - offset);
  memcpy(out, source.data() + offset, received);
  return true;
}

int main() {
  StorageService storage;
  Logger logger;
  YapRuntimeService runtime;
  runtime.begin(storage, logger);
  YapPackageInfo package;
  package.sectionCount = 1;
  package.sections[0].type = YapPackageService::TYPE_LUA_SOURCE;
  package.manifest.requestedMemory = 32768;
  strlcpy(package.manifest.entryPoint, "main", sizeof(package.manifest.entryPoint));
  auto start = [&](const char* code) {
    source = code;
    package.sections[0].length = source.size();
    return runtime.start(package);
  };
  auto drain = [&]() {
    int ticks = 0;
    while (runtime.running() && ++ticks < 2000) {
      runtime.update();
      testMillis += 2;
    }
    assert(!runtime.running());
    assert(runtime.result().remainingLuaBytes == 0);
  };
  for (int repeat = 0; repeat < 100; ++repeat) {
    assert(start("function main() osesp32.ui.label('Hello') end"));
    drain();
    assert(runtime.result().status == YapRuntimeStatus::Success);
    assert(!strcmp(runtime.result().label, "Hello"));
  }
  assert(start("function main() while true do pcall(function() while true do end end) end end"));
  drain();
  assert(runtime.result().status == YapRuntimeStatus::LimitExceeded);
  assert(start("function main() local t={} while true do t[#t+1]=string.rep('x',1024) end end"));
  drain();
  assert(runtime.result().status == YapRuntimeStatus::OutOfMemory);
  assert(!start("function main("));
  assert(runtime.result().status == YapRuntimeStatus::CompileError);
  assert(runtime.result().remainingLuaBytes == 0);
  assert(start("local x=1"));
  drain();
  assert(runtime.result().status == YapRuntimeStatus::EntryMissing);
  assert(start("function main() osesp32.ui.label('before') osesp32.sleep(100) osesp32.ui.label('after') end"));
  runtime.update();
  runtime.update();
  assert(runtime.running());
  assert(!strcmp(runtime.result().label, "before"));
  testMillis += 99;
  runtime.update();
  assert(!strcmp(runtime.result().label, "before"));
  testMillis += 1;
  drain();
  assert(!strcmp(runtime.result().label, "after"));
  assert(start("function main() while true do osesp32.sleep(1000) end end"));
  runtime.update(); runtime.update();
  runtime.stop(); runtime.stop();
  assert(runtime.result().status == YapRuntimeStatus::Cancelled);
  assert(runtime.result().remainingLuaBytes == 0);
  assert(start("function main() assert(setmetatable == nil and coroutine == nil) end"));
  drain();
  assert(runtime.result().status == YapRuntimeStatus::Success);
  assert(start("function main() osesp32.sleep(60000) end"));
  assert(!runtime.start(package)); // Busy must preserve the active VM.
  runtime.update(); runtime.update();
  simulateRemoved = true; storage.update();
  runtime.update();
  assert(runtime.result().status == YapRuntimeStatus::IoError);
  assert(runtime.result().remainingLuaBytes == 0);
  simulateRemoved = false; storage.update();
  source = "function main() end";
  package.sections[0].length = source.size() + 200;
  assert(!runtime.start(package));
  assert(runtime.result().status == YapRuntimeStatus::IoError);
  assert(runtime.result().remainingLuaBytes == 0);
  testMillis = UINT32_MAX - 10;
  assert(start("function main() osesp32.sleep(20) osesp32.ui.label('wrap') end"));
  drain();
  assert(!strcmp(runtime.result().label, "wrap"));
  AppLifecycle lifecycle;
  assert(lifecycle.begin());
  assert(!lifecycle.begin());
  lifecycle.requestExit();
  lifecycle.prepared(true);
  assert(lifecycle.state() == AppLifecycle::State::Stopping);
  lifecycle.stopped(); lifecycle.restored();
  assert(!lifecycle.active());
  assert(lifecycle.begin()); lifecycle.prepared(false);
  lifecycle.stopped(); lifecycle.restored();
  assert(!lifecycle.active());
  puts("PASS: 100 runs, quotas, pcall loop, errors, sleep/wrap, cancel, SD removal, lifecycle");
}
