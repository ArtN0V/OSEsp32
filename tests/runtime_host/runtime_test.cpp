#include "../../src/runtime/YapRuntimeService.h"
#include "../../src/runtime/AppLifecycle.h"
#include "../../src/ui/SystemExitGesture.h"
#include "../../src/services/DateTimeService.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <map>
#include <set>
#include <fstream>
#include <iterator>
#include <strings.h>

uint32_t testMillis = 0;
TestESP ESP;
int64_t testMicros=0;
bool SystemSettingsService::loadClock(uint64_t& utc,int16_t& zone) const { utc=1700000000; zone=0; return true; }
bool SystemSettingsService::saveClock(uint64_t,int16_t) const { return true; }
static std::string source;
static bool simulateRemoved = false;
static std::map<std::string,std::string> files;
static std::set<std::string> directories;
static int failRename=0;
StorageService::StorageService() { mounted_ = true; }
void StorageService::update() { if (mounted_==simulateRemoved) ++generation_; mounted_ = !simulateRemoved; }
void Logger::info(const char*, const char*, ...) {}
bool StorageService::isYapPath(const char* path) {
  const char* ext=strrchr(path,'.'); return ext && !strcasecmp(ext,".yap");
}
bool StorageService::computeFileCrc32(const char* path,uint32_t offset,uint32_t length,uint32_t& crc,uint32_t zeroOffset,uint32_t zeroLength) const {
  if (!files.count(path) || offset>files[path].size() || length>files[path].size()-offset) return false;
  uint32_t value=0xffffffff;
  for (uint32_t i=offset;i<offset+length;++i) {
    uint8_t byte=static_cast<uint8_t>(files[path][i]);
    if (i>=zeroOffset && i-zeroOffset<zeroLength) byte=0;
    value^=byte;
    for (int bit=0;bit<8;++bit) value=(value>>1)^(0xedb88320u & (0u-(value&1)));
  }
  crc=value^0xffffffff; return true;
}
bool StorageService::exists(const char* path) const { return files.count(path)||directories.count(path); }
bool StorageService::removePath(const char* path) { files.erase(path); return true; }
bool StorageService::renamePath(const char* from,const char* to) {
  if (failRename && !--failRename) return false;
  if (!files.count(from) || exists(to)) return false;
  files[to]=files[from]; files.erase(from); return true;
}
bool StorageService::makeDirectory(const char* path) { directories.insert(path); return true; }
uint64_t StorageService::freeBytes() const { return 100*1024*1024; }
bool StorageService::fileSize(const char* path,uint32_t& size) const {
  if (!files.count(path)) return false;
  size=files[path].size(); return true;
}
bool StorageService::writeRange(const char* path,uint32_t offset,const uint8_t* data,size_t length,bool truncate) {
  if (truncate) files[path].clear();
  if (!files.count(path) || offset>files[path].size()) return false;
  auto& value=files[path]; if (offset+length>value.size()) value.resize(offset+length);
  if (length) memcpy(&value[offset],data,length); return true;
}
bool StorageService::readFile(const char* path,char* out,size_t capacity,size_t& length,bool) {
  if (!files.count(path) || files[path].size()>=capacity) return false;
  length=files[path].size(); memcpy(out,files[path].data(),length); out[length]=0; return true;
}
bool StorageService::listDirectoryPage(const char*,uint16_t,StorageEntry*,uint8_t,uint8_t& count,uint16_t& total) {
  count=0; total=0; return true;
}
bool StorageService::readFileRange(const char* path, uint32_t offset, uint8_t* out,
                                  size_t length, size_t& received) const {
  const auto& data=*path ? files[path] : source;
  if (offset > data.size()) return false;
  received = std::min(length, data.size() - offset);
  memcpy(out, data.data() + offset, received);
  return true;
}

int main(int argc,char** argv) {
  StorageService storage;
  Logger logger;
  YapPackageService parser; parser.begin(storage,logger);
  for (int i=1;i<argc;++i) {
    std::ifstream input(argv[i],std::ios::binary);
    files["/fixture.yap"]=std::string(std::istreambuf_iterator<char>(input),{});
    if (i==1) files["/valid.yap"]=files["/fixture.yap"];
    YapPackageInfo parsed;
    YapError error=parser.inspect("/fixture.yap",parsed);
    assert((error==YapError::None)==(i==1));
  }
  SystemSettingsService settings; DateTimeService clock; clock.begin(settings);
  testMicros=60LL*86400*1000000;
  assert(clock.utcNow()==1700000000+60LL*86400);
  assert(clock.setUtc(3600,-120));
  struct tm output={}; assert(!clock.localTime(output));
  assert(clock.setUtc(3600,60)); assert(clock.localTime(output) && output.tm_hour==2);
  struct tm date={}; date.tm_year=124; date.tm_mon=1; date.tm_mday=30;
  assert(!clock.setLocal(date,0)); date.tm_mday=29; assert(clock.setLocal(date,0));
  date.tm_year=125; assert(!clock.setLocal(date,0));
  puts("PASS: real package parser rejects malformed fixtures; clock rollover/calendar");
  YapRuntimeService runtime;
  runtime.begin(storage, logger);
  if (argc>1) {
    YapPackageInfo demo;
    assert(parser.inspect("/valid.yap",demo)==YapError::None);
    assert(runtime.start(demo));
    for (int tick=0;tick<100 && runtime.request()!=YapRuntimeService::Request::Event;++tick) runtime.update();
    assert(runtime.running() && runtime.request()==YapRuntimeService::Request::Event);
    assert(strstr(runtime.result().label,"Hello from a streamed"));
    runtime.postEvent(1); runtime.update();
    assert(runtime.request()==YapRuntimeService::Request::Text);
    runtime.reply("Test edit"); runtime.update();
    assert(runtime.request()==YapRuntimeService::Request::Event);
    runtime.postEvent(6); runtime.update();
    assert(!runtime.running() && runtime.result().exitedByApp);
  }
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
  assert(runtime.running()); // Retain RAM until the user chooses Retry/Close.
  runtime.stop(YapRuntimeStatus::IoError);
  assert(runtime.result().remainingLuaBytes==0);
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
  for (int repeat = 0; repeat < 100; ++repeat) {
    assert(start("function main() osesp32.sleep(1) osesp32.ui.label('exit') "
                 "pcall(function() osesp32.exit() end) error('must not resume') end"));
    drain();
    assert(runtime.result().status == YapRuntimeStatus::Success);
    assert(runtime.result().exitedByApp);
    assert(!strcmp(runtime.result().label, "exit"));
  }
  assert(start("osesp32.exit(); error('top-level code must not resume')"));
  drain();
  assert(runtime.result().exitedByApp);
  assert(start("function main() end"));
  drain();
  assert(!runtime.result().exitedByApp);

  strlcpy(package.manifest.appId,"test.app",sizeof(package.manifest.appId));
  package.manifest.capabilities=YapPrivateRead|YapPrivateWrite|YapDocumentsOpen|YapDocumentsCreate|YapDocumentsReplace;
  package.manifest.associationCount=1;
  strlcpy(package.manifest.associations[0],"txt",9);
  assert(start("function main() local f=osesp32.fs "
    "local h=assert(f.open('data:/test.txt','w')); assert(f.write(h,'hello\\0world')); "
    "assert(f.close(h)); h=assert(f.open('data:/test.txt','r')); assert(f.size(h)==11); "
    "assert(f.read(h,512)=='hello\\0world'); assert(f.read(h,1)==''); "
    "assert(f.seek(h,6)); assert(f.read(h,5)=='world'); assert(f.close(h)); "
    "local ok,e=f.read(h,1); assert(ok==nil and e=='invalid_handle'); "
    "assert(f.open('data:/../escape','w')==nil); "
    "assert(f.open('/Documents/a.txt','r')==nil); "
    "assert(f.open('data:/','w')==nil); end"));
  drain(); assert(runtime.result().status==YapRuntimeStatus::Success);
  assert(files["/OSEsp32/Data/test.app/test.txt"]==std::string("hello\0world",11));
  assert(start("function main() osesp32.ui.button(1,'Exit'); assert(osesp32.ui.wait()==1); "
    "local value=osesp32.ui.text('Hi'); assert(value=='Привет'); "
    "local h=assert(osesp32.documents.open()); assert(osesp32.fs.read(h,3)=='old'); "
    "assert(osesp32.fs.write(h,'evil')==nil); assert(osesp32.fs.close(h)); osesp32.exit() end"));
  runtime.update(); runtime.update();
  assert(runtime.request()==YapRuntimeService::Request::Event);
  runtime.postEvent(1); runtime.update();
  assert(runtime.request()==YapRuntimeService::Request::Text);
  runtime.reply("Привет"); runtime.update();
  assert(runtime.request()==YapRuntimeService::Request::Open);
  files["/Documents/a.txt"]="old";
  int grant=runtime.files().grant("/Documents/a.txt","r"); assert(grant);
  runtime.reply(nullptr,grant); drain();
  assert(runtime.result().exitedByApp);
  assert(start("function main() local h,e=osesp32.documents.save('a.txt'); assert(h==nil and e=='cancelled') end"));
  runtime.update(); runtime.update(); runtime.reply(nullptr,0,"cancelled"); drain();
  assert(runtime.result().status==YapRuntimeStatus::Success);

  package.manifest.apiMinor=2;
  package.manifest.launchMode=YapLaunchMode::Fullscreen;
  assert(start("function main() local u=osesp32.ui; u.clear(); "
    "u.label(100,'Display',0,0,100,24,'status'); u.button(1,'One',0,28,60,28); "
    "u.toggle(2,'Flag',true,64,28,100,28); u.text_field(3,'Text',0,60,120,28); "
    "u.list(4,{'A','B'},124,60,120,60); assert(u.value(2)==true); "
    "local id,kind=u.wait(); assert(id==1 and kind=='tap'); "
    "assert(u.confirm('Continue?')==true); osesp32.exit() end"));
  runtime.update(); runtime.update();
  assert(runtime.request()==YapRuntimeService::Request::Event);
  assert(runtime.widgetCount()==5 && runtime.viewportWidth()==320);
  runtime.postUiEvent(1,YapRuntimeService::UiEventKind::Tap); runtime.update();
  assert(runtime.request()==YapRuntimeService::Request::Confirm);
  runtime.reply(nullptr,1); drain(); assert(runtime.result().exitedByApp);
  assert(start("function main() local u=osesp32.ui; u.toggle(2,'Flag',false,0,0,100,28); "
    "u.text_field(3,'Old',0,30,100,28); u.list(4,{'A','B'},0,60,100,60); "
    "local i,k,v=u.wait(); assert(i==2 and k=='change' and v==true and u.value(2)==true); "
    "i,k,v=u.wait(); assert(i==3 and k=='change' and v=='Новый' and u.value(3)=='Новый'); "
    "i,k,v=u.wait(); assert(i==4 and k=='change' and v==2 and u.value(4)==2); osesp32.exit() end"));
  runtime.update(); runtime.update();
  runtime.postUiEvent(2,YapRuntimeService::UiEventKind::Change,1); runtime.update();
  runtime.postUiEvent(3,YapRuntimeService::UiEventKind::Change,0,"Новый"); runtime.update();
  runtime.postUiEvent(4,YapRuntimeService::UiEventKind::Change,2); drain();
  assert(runtime.result().exitedByApp);
  assert(start("function main() osesp32.ui.timer(9,50,false); local i,k=osesp32.ui.wait(); "
    "assert(i==9 and k=='timer') end"));
  runtime.update(); runtime.update(); testMillis+=50; drain();
  assert(runtime.result().status==YapRuntimeStatus::Success);
  assert(start("function main() osesp32.ui.button(1,'Bad',319,0,20,20) end"));
  drain(); assert(runtime.result().status==YapRuntimeStatus::ExecutionError);
  assert(start("function main() for i=1,25 do osesp32.ui.button(i,'X',0,0,20,20) end end"));
  drain(); assert(runtime.result().status==YapRuntimeStatus::ExecutionError);
  assert(start("function main() osesp32.ui.list(1,{'1','2','3','4','5','6','7'},0,0,80,80) end"));
  drain(); assert(runtime.result().status==YapRuntimeStatus::ExecutionError);
  package.manifest.apiMinor=3;
  package.manifest.launchMode=YapLaunchMode::Exclusive;
  package.manifest.requestedMemory=24576;
  constexpr uint8_t canvasCycles=64;
  assert(start("function main() local c=osesp32.canvas; for i=1,64 do "
    "local s=assert(c.probe('i4')); "
    "assert(s.format=='i4' and s.buffer_bytes==32704 and s.frame_count==30); "
    "s=assert(c.release()); assert(s.format=='released') end osesp32.exit() end"));
  runtime.update(); runtime.update();
  YapRuntimeService::CanvasStats canvasStats;
  for (uint8_t cycle=0;cycle<canvasCycles;++cycle) {
    for (uint8_t tick=0;runtime.running() &&
         runtime.request()==YapRuntimeService::Request::None && tick<50;++tick)
      runtime.update();
    assert(runtime.request()==YapRuntimeService::Request::CanvasProbe);
    canvasStats={}; strlcpy(canvasStats.format,"i4",sizeof(canvasStats.format));
    canvasStats.bufferBytes=32704; canvasStats.frameCount=30;
    runtime.replyCanvas(canvasStats); runtime.update();
    for (uint8_t tick=0;runtime.running() &&
         runtime.request()==YapRuntimeService::Request::None && tick<50;++tick)
      runtime.update();
    assert(runtime.request()==YapRuntimeService::Request::CanvasRelease);
    canvasStats={}; strlcpy(canvasStats.format,"released",sizeof(canvasStats.format));
    runtime.replyCanvas(canvasStats); runtime.update();
  }
  drain(); assert(runtime.result().exitedByApp);
  package.manifest.launchMode=YapLaunchMode::Windowed;
  assert(start("function main() local s,e=osesp32.canvas.probe('i8'); "
    "assert(s==nil and e=='exclusive_required') end"));
  drain(); assert(runtime.result().status==YapRuntimeStatus::Success);
  package.manifest.apiMinor=4;
  package.manifest.launchMode=YapLaunchMode::Exclusive;
  assert(start("function main() local c=osesp32.canvas; assert(c.create(320,176)); "
    "assert(c.clear(15)); assert(c.line(1,2,30,40,3,5)); "
    "local id,k,x,y=osesp32.ui.wait(); assert(id==0 and k=='canvas_down' and x==12 and y==34); "
    "assert(c.release()); osesp32.exit() end"));
  runtime.update(); runtime.update();
  assert(runtime.request()==YapRuntimeService::Request::CanvasCreate);
  assert(runtime.canvasCommand().width==320 && runtime.canvasCommand().height==176);
  runtime.replyCanvasCommand(); runtime.update();
  assert(runtime.request()==YapRuntimeService::Request::CanvasClear);
  assert(runtime.canvasCommand().color==15);
  runtime.replyCanvasCommand(); runtime.update();
  assert(runtime.request()==YapRuntimeService::Request::CanvasLine);
  assert(runtime.canvasCommand().x2==30 && runtime.canvasCommand().y2==40 &&
         runtime.canvasCommand().color==3 && runtime.canvasCommand().thickness==5);
  runtime.replyCanvasCommand(); runtime.update();
  assert(runtime.request()==YapRuntimeService::Request::Event);
  runtime.postCanvasEvent(YapRuntimeService::UiEventKind::CanvasDown,12,34);
  runtime.update();
  assert(runtime.request()==YapRuntimeService::Request::CanvasRelease);
  canvasStats={}; strlcpy(canvasStats.format,"released",sizeof(canvasStats.format));
  runtime.replyCanvas(canvasStats); drain(); assert(runtime.result().exitedByApp);
  package.manifest.apiMinor=5;
  assert(start("function main() local c=osesp32.canvas; assert(c.create(320,176)); "
    "assert(c.load_bmp(23)); assert(c.save_bmp(24)); assert(c.release()); osesp32.exit() end"));
  runtime.update(); runtime.update();
  assert(runtime.request()==YapRuntimeService::Request::CanvasCreate);
  runtime.replyCanvasCommand(); runtime.update();
  assert(runtime.request()==YapRuntimeService::Request::CanvasLoadBmp);
  assert(runtime.canvasCommand().handle==23);
  runtime.replyCanvasCommand(); runtime.update();
  assert(runtime.request()==YapRuntimeService::Request::CanvasSaveBmp);
  assert(runtime.canvasCommand().handle==24);
  runtime.replyCanvasCommand(); runtime.update();
  assert(runtime.request()==YapRuntimeService::Request::CanvasRelease);
  canvasStats={}; strlcpy(canvasStats.format,"released",sizeof(canvasStats.format));
  runtime.replyCanvas(canvasStats); drain(); assert(runtime.result().exitedByApp);
  package.manifest.apiMinor=1;
  package.manifest.launchMode=YapLaunchMode::Windowed;
  package.manifest.requestedMemory=32768;
  package.manifest.associationCount=2;
  strlcpy(package.manifest.associations[1],"bmp",9);

  AppStorageService fs;
  fs.begin(storage,package);
  const char* invalid[]={"../x","/x","a//b","a/./b","a/../b","a.","a ","a\\b","\xc0\xaf","\xed\xa0\x80","\xf4\x90\x80\x80","\xe2\x82"};
  for (auto* name:invalid) assert(!AppStorageService::validRelative(name));
  assert(AppStorageService::validRelative("папка/файл.txt"));
  assert(!fs.grant("/OSEsp32/Notes/a.txt","r"));
  files["/OSEsp32/Wallpapers/source.bmp"]="BMtest";
  int wallpaper=fs.grant("/OSEsp32/Wallpapers/source.bmp","r"); assert(wallpaper);
  assert(fs.close(wallpaper));
  assert(!fs.grant("/OSEsp32/Wallpapers/source.bmp","w"));
  assert(fs.permitsDocumentDirectory("/OSEsp32","r"));
  assert(fs.permitsDocumentDirectory("/OSEsp32/Wallpapers","r"));
  assert(fs.permitsDocumentDirectory("/OSEsp32/Wallpapers/Archive","r"));
  assert(!fs.permitsDocumentDirectory("/OSEsp32/Notes","r"));
  assert(!fs.permitsDocumentDirectory("/OSEsp32","w"));
  assert(!fs.grant("/Documents/a.bmp","w"));
  int handles[4]; for (int& h:handles) { h=fs.open("data:/test.txt","r"); assert(h); }
  assert(!fs.open("data:/test.txt","r"));
  for (int h:handles) assert(fs.close(h));
  int old=fs.open("data:/test.txt","r"); assert(old);
  simulateRemoved=true; storage.update(); size_t count=0; uint8_t buffer[512];
  assert(!fs.read(old,buffer,1,count));
  simulateRemoved=false; storage.update();
  assert(!fs.read(old,buffer,1,count));
  fs.begin(storage,package);
  int fresh=fs.open("data:/test.txt","r"); assert(fresh && fresh!=old);
  assert(!fs.read(old,buffer,1,count)); fs.end();

  // Cut at either rename, before commit, and after destination installation.
  for (int phase=0;phase<4;++phase) {
    assert(AppStorageService::recover(storage));
    files["/Documents/a.txt"]="old";
    fs.begin(storage,package);
    int h=fs.grant("/Documents/a.txt","w"); assert(h);
    assert(fs.write(h,reinterpret_cast<const uint8_t*>("new"),3));
    assert(files["/Documents/a.txt"]=="old");
    if (phase==1 || phase==2) { failRename=phase; assert(!fs.close(h)); }
    if (phase==3) {
      assert(storage.renamePath("/Documents/a.txt","/OSEsp32/Transactions/0.old"));
      assert(storage.renamePath("/OSEsp32/Transactions/0.data","/Documents/a.txt"));
    }
    fs.invalidate(); fs.end(); failRename=0;
    assert(AppStorageService::recover(storage));
    assert(files["/Documents/a.txt"]==(phase==3 ? "new" : "old"));
  }
  fs.begin(storage,package);
  int h=fs.grant("/Documents/a.txt","w"); assert(h);
  assert(fs.write(h,reinterpret_cast<const uint8_t*>("committed"),9)); assert(fs.close(h));
  assert(files["/Documents/a.txt"]=="committed");
  h=fs.grant("/Documents/a.txt","w"); assert(h); assert(fs.write(h,buffer,1));
  fs.end(); assert(files["/Documents/a.txt"]=="committed");
  package.manifest.capabilities=0; fs.begin(storage,package);
  assert(!fs.open("data:/test.txt","r")); assert(!strcmp(fs.error(),"permission_denied"));
  assert(!fs.grant("/Documents/a.txt","r")); fs.end();
  files["/OSEsp32/Transactions/0.txn"]="YTX1:00000000:/Documents/a.txt";
  files["/OSEsp32/Transactions/0.data"]="untrusted";
  assert(!AppStorageService::recover(storage));
  assert(files.count("/OSEsp32/Transactions/0.txn") && files.count("/OSEsp32/Transactions/0.data"));
  files.erase("/OSEsp32/Transactions/0.txn"); files.erase("/OSEsp32/Transactions/0.data");
  // Package resources are bounded by the section, not the entire container.
  strcpy(package.path,"/resource.yap"); package.sectionCount=2;
  package.sections[1].type=YapPackageService::TYPE_RESOURCE;
  package.sections[1].offset=0; package.sections[1].length=67;
  files[package.path]=std::string("message.txt")+std::string(53,'\0')+"abcSECRET";
  fs.begin(storage,package); h=fs.open("app:/message.txt","r"); assert(h);
  assert(fs.read(h,buffer,512,count) && count==3 && !memcmp(buffer,"abc",3));
  assert(!fs.write(h,buffer,1)); fs.end();
  puts("PASS: file namespace/capabilities, UTF-8, handles, removal, transactions, UI requests");

  SystemExitGesture gesture;
  assert(!gesture.update(true, 0, 0, 0)); // Initial held tap is ignored.
  assert(!gesture.update(true, 0, 0, 5000));
  assert(!gesture.update(false, 0, 0, 5001));
  assert(!gesture.update(true, 31, 31, 6000));
  assert(!gesture.update(true, 31, 31, 7999));
  assert(gesture.update(true, 31, 31, 8000));
  assert(!gesture.update(true, 31, 31, 12000)); // One exit per hold.
  gesture.update(false, 0, 0, 12001);
  gesture.update(true, 0, 0, 13000);
  assert(!gesture.update(true, 32, 0, 14999)); // Leaving cancels elapsed time.
  assert(!gesture.update(true, 0, 0, 15000));
  assert(!gesture.update(true, 0, 0, 16999));
  assert(gesture.update(true, 0, 0, 17000));
  gesture.update(false, 0, 0, UINT32_MAX - 1000);
  gesture.update(true, 0, 0, UINT32_MAX - 999);
  assert(!gesture.update(true, 0, 0, 999));
  assert(gesture.update(true, 0, 0, 1000));
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
  puts("PASS: 100 runs + 100 app exits, quotas, errors, sleep, cancel, SD, lifecycle, exit gesture");
}
