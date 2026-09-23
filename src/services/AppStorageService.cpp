#include "AppStorageService.h"
#include <strings.h>
#include <limits.h>

namespace {
uint32_t journalCrc(const char* text) {
  uint32_t value=0xffffffffu;
  for (;*text;++text) {
    value^=static_cast<uint8_t>(*text);
    for (unsigned bit=0;bit<8;++bit) value=(value>>1)^(0xedb88320u & (0u-(value&1)));
  }
  return value^0xffffffffu;
}
}

bool AppStorageService::validRelative(const char* path, bool emptyAllowed) {
  if (!path || (!*path && !emptyAllowed) || strlen(path) > 127) return false;
  const char* part = path;
  const unsigned char* p = reinterpret_cast<const unsigned char*>(path);
  while (*p) {
    unsigned char c = *p++;
    if (c < 0x20 || c == 0x7f || strchr("\\:*?\"<>|", c)) return false;
    if (c == '/') {
      size_t n = reinterpret_cast<const char*>(p) - part - 1;
      if (!n || n > 48 || part[n-1] == '.' || part[n-1] == ' ') return false;
      part = reinterpret_cast<const char*>(p);
    } else if (c >= 0x80) {
      unsigned n; uint32_t cp, minimum;
      if ((c & 0xe0) == 0xc0) { n=1; cp=c&31; minimum=0x80; }
      else if ((c & 0xf0) == 0xe0) { n=2; cp=c&15; minimum=0x800; }
      else if ((c & 0xf8) == 0xf0) { n=3; cp=c&7; minimum=0x10000; }
      else return false;
      while (n--) { if ((*p & 0xc0) != 0x80) return false; cp=(cp<<6)|(*p++&63); }
      if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
    }
  }
  size_t n = strlen(part);
  return (!*path && emptyAllowed) || (n && n <= 48 && part[n-1] != '.' && part[n-1] != ' ');
}

void AppStorageService::transactionPaths(uint8_t slot, char* data, char* backup, char* journal) {
  snprintf(data, 64, "/OSEsp32/Transactions/%u.data", slot);
  snprintf(backup, 64, "/OSEsp32/Transactions/%u.old", slot);
  snprintf(journal, 64, "/OSEsp32/Transactions/%u.txn", slot);
}

bool AppStorageService::recover(StorageService& storage) {
  if (!storage.mounted()) return false;
  bool ok = true;
  for (uint8_t slot=0; slot<MAX_HANDLES; ++slot) {
    char data[64], backup[64], journal[64], record[160]; size_t read=0;
    transactionPaths(slot,data,backup,journal);
    if (!storage.exists(journal)) {
      // An unexplained backup is preserved, never recycled automatically.
      if (storage.exists(backup)) { ok=false; continue; }
      if (!storage.removePath(data)) ok=false;
      continue;
    }
    unsigned long crc=0;
    if (!storage.readFile(journal,record,sizeof(record),read) || read<15 || read!=strlen(record) ||
        strncmp(record,"YTX1:",5) || record[13]!=':' || sscanf(record+5,"%8lx",&crc)!=1) { ok=false; continue; }
    const char* target=record+14;
    if (journalCrc(target)!=crc ||
        target[0] != '/' || !validRelative(target+1) ||
        !strcasecmp(target,"/OSEsp32") ||
        (!strncasecmp(target,"/OSEsp32/",9) && strncmp(target,"/OSEsp32/Data/",14))) { ok=false; continue; }
    if (storage.exists(backup)) {
      // Rename only happens after all new bytes have been flushed. If target
      // exists it is the new complete file; otherwise restore the old one.
      if (!storage.exists(target)) {
        if (!storage.renamePath(backup,target)) { ok=false; continue; }
      } else if (!storage.removePath(backup)) { ok=false; continue; }
    }
    if (!storage.removePath(data) || !storage.removePath(journal)) ok=false;
  }
  return ok;
}

void AppStorageService::begin(StorageService& storage, const YapPackageInfo& package) {
  end(); storage_=&storage; package_=package; generation_=storage.generation();
  error_="ok";
}
void AppStorageService::invalidate() {
  for (auto& h:handles_) h = {};
  generation_=UINT32_MAX;
}
void AppStorageService::end() {
  for (auto& h:handles_) if (h.token) close(h.token,false);
  for (auto& h:handles_) h = {};
  storage_=nullptr;
}
bool AppStorageService::ready() {
  if (!storage_ || !storage_->mounted() || generation_ != storage_->generation())
    return fail("storage_removed");
  error_="ok"; return true;
}
AppStorageService::Handle* AppStorageService::handle(int token) {
  if (!ready()) return nullptr;
  for (auto& h:handles_) if (h.token && h.token==token) return &h;
  fail("invalid_handle"); return nullptr;
}
bool AppStorageService::resolve(const char* path, char* target, bool writeAccess) {
  if (!ready()) return false;
  if (!path || strncmp(path,"data:/",6) || !validRelative(path+6,true)) return fail("invalid_path");
  const uint32_t capability=writeAccess ? YapPrivateWrite : YapPrivateRead;
  if (!(package_.manifest.capabilities & capability)) return fail("permission_denied");
  int n=snprintf(target,129,"/OSEsp32/Data/%s%s%s",package_.manifest.appId,
                 path[6] ? "/" : "",path+6);
  return (n>0 && n<129) || fail("invalid_path");
}
bool AppStorageService::permitsDocument(const char* path, const char* mode) const {
  if (!path || path[0]!='/' || !validRelative(path+1) ||
      !strncasecmp(path,"/OSEsp32/",9) || !strcasecmp(path,"/OSEsp32")) return false;
  uint32_t cap = !strcmp(mode,"r") ? YapDocumentsOpen :
                 !strcmp(mode,"x") ? YapDocumentsCreate :
                 !strcmp(mode,"w") ? YapDocumentsReplace : 0;
  if (!cap || !(package_.manifest.capabilities&cap)) return false;
  const char* ext=strrchr(path,'.');
  if (!ext) return false;
  for (uint8_t i=0;i<package_.manifest.associationCount;++i)
    if (!strcasecmp(ext+1,package_.manifest.associations[i])) return true;
  return false;
}
int AppStorageService::grant(const char* path,const char* mode) {
  if (!ready()) return 0;
  if (!permitsDocument(path,mode)) { fail("permission_denied"); return 0; }
  if (!strcmp(mode,"w") && !storage_->exists(path)) { fail("not_found"); return 0; }
  return openResolved(path,mode);
}
int AppStorageService::open(const char* path,const char* mode) {
  if (!ready()) return 0;
  if (!strcmp(mode,"r") && path && !strncmp(path,"app:/",5)) {
    if (!validRelative(path+5)) { fail("invalid_path"); return 0; }
    for (uint8_t i=0;i<package_.sectionCount;++i) {
      const YapSection& s=package_.sections[i];
      if (s.type!=YapPackageService::TYPE_RESOURCE || s.length<64) continue;
      uint8_t name[64]; size_t count=0;
      if (!storage_->readFileRange(package_.path,s.offset,name,64,count) || count!=64) { fail("io_error"); return 0; }
      if (!memchr(name,0,64) || !validRelative(reinterpret_cast<char*>(name))) { fail("bad_resource"); return 0; }
      if (!strcmp(path+5,reinterpret_cast<char*>(name)))
        return openResolved(package_.path,"r",s.offset+64,s.length-64);
    }
    fail("not_found"); return 0;
  }
  char target[129];
  if (!resolve(path,target,strcmp(mode,"r")!=0)) return 0;
  if (!path[6]) { fail("invalid_path"); return 0; }
  if (!strcmp(mode,"a") && !(package_.manifest.capabilities & YapPrivateRead)) {
    fail("permission_denied"); return 0;
  }
  char root[80]; snprintf(root,sizeof(root),"/OSEsp32/Data/%s",package_.manifest.appId);
  if (strcmp(mode,"r") && !storage_->makeDirectory(root)) { fail("io_error"); return 0; }
  return openResolved(target,mode);
}
int AppStorageService::openResolved(const char* path,const char* mode,uint32_t base,uint32_t length) {
  const bool writeAccess=strcmp(mode,"r");
  if (writeAccess && strcmp(mode,"w") && strcmp(mode,"x") && strcmp(mode,"a")) { fail("invalid_mode"); return 0; }
  uint8_t slot=0;
  while (slot<MAX_HANDLES && handles_[slot].token) ++slot;
  if (slot==MAX_HANDLES || nextToken_==INT_MAX) { fail("too_many_handles"); return 0; }
  for (const auto& h:handles_) if (h.token && !strcasecmp(h.target,path) && (writeAccess || h.writable)) {
    fail("busy"); return 0;
  }
  Handle h;
  strlcpy(h.target,path,sizeof(h.target));
  h.base=base; h.writable=writeAccess; h.createOnly=!strcmp(mode,"x");
  if (!writeAccess) {
    strlcpy(h.path,path,sizeof(h.path));
    if (length==UINT32_MAX && !storage_->fileSize(path,length)) { fail("not_found"); return 0; }
    h.length=length;
  } else {
    uint32_t existingSize=0;
    if (storage_->exists(path) && !storage_->fileSize(path,existingSize)) { fail("invalid_path"); return 0; }
    // Copy-on-open append is deliberately small; larger edits must stream into
    // a separate transactional handle with explicit scheduler yields.
    if (!strcmp(mode,"a") && existingSize>4096) { fail("append_too_large"); return 0; }
    if (h.createOnly && storage_->exists(path)) { fail("already_exists"); return 0; }
    if (storage_->freeBytes()<RESERVE+512) { fail("no_space"); return 0; }
    char backup[64], journal[64], data[64]; transactionPaths(slot,data,backup,journal);
    if (storage_->exists(backup) || storage_->exists(journal)) { fail("recovery_required"); return 0; }
    char record[160]; snprintf(record,sizeof(record),"YTX1:%08lx:%s",static_cast<unsigned long>(journalCrc(path)),path);
    if (!storage_->writeRange(journal,0,reinterpret_cast<const uint8_t*>(record),strlen(record),true) ||
        !storage_->writeRange(data,0,nullptr,0,true)) { fail("io_error"); return 0; }
    strlcpy(h.path,data,sizeof(h.path));
    if (!strcmp(mode,"a") && storage_->exists(path)) {
      uint32_t size=0;
      if (!storage_->fileSize(path,size) || size>MAX_FILE_SIZE || storage_->freeBytes()<RESERVE+size) { fail("quota_exceeded"); return 0; }
      uint8_t bytes[512];
      while (h.length<size) {
        size_t n=0, wanted=min(static_cast<uint32_t>(sizeof(bytes)),size-h.length);
        if (!storage_->readFileRange(path,h.length,bytes,wanted,n) || n!=wanted || !storage_->writeRange(data,h.length,bytes,n)) { fail("io_error"); return 0; }
        h.length+=n;
        delay(0);
      }
      h.cursor=h.length;
    }
  }
  h.token=nextToken_++; handles_[slot]=h; return h.token;
}
bool AppStorageService::read(int token,uint8_t* data,size_t wanted,size_t& count) {
  count=0; Handle* h=handle(token); if (!h) return false;
  if (wanted>512) return fail("transfer_too_large");
  wanted=min(wanted,static_cast<size_t>(h->length-h->cursor));
  if (!wanted) return true;
  if (!storage_->readFileRange(h->path,h->base+h->cursor,data,wanted,count) || count!=wanted) return fail("io_error");
  h->cursor+=count; return true;
}
bool AppStorageService::write(int token,const uint8_t* data,size_t count) {
  Handle* h=handle(token); if (!h) return false;
  if (!h->writable) return fail("permission_denied");
  if (count>512) return fail("transfer_too_large");
  if (h->cursor>MAX_FILE_SIZE-count) return fail("quota_exceeded");
  if (storage_->freeBytes()<RESERVE+count) return fail("no_space");
  if (!storage_->writeRange(h->path,h->cursor,data,count)) return fail("io_error");
  h->cursor+=count; if (h->cursor>h->length) h->length=h->cursor; return true;
}
bool AppStorageService::seek(int token,uint32_t position) {
  Handle* h=handle(token); if (!h) return false;
  if (position>h->length) return fail("invalid_offset");
  h->cursor=position; return true;
}
bool AppStorageService::size(int token,uint32_t& length) {
  Handle* h=handle(token); if (!h) return false;
  length=h->length; return true;
}
bool AppStorageService::flush(int token) { return handle(token)!=nullptr; }
bool AppStorageService::close(int token,bool commit) {
  Handle* h=handle(token); if (!h) return false;
  if (!h->writable) { *h={}; return true; }
  const uint8_t slot=h-handles_;
  char data[64],backup[64],journal[64]; transactionPaths(slot,data,backup,journal);
  bool ok=true;
  if (commit) {
    if (storage_->exists(h->target)) {
      if (h->createOnly) return fail("already_exists");
      ok=storage_->renamePath(h->target,backup);
    }
    if (ok) ok=storage_->renamePath(data,h->target);
    if (ok) ok=storage_->removePath(backup);
  } else ok=storage_->removePath(data);
  if (ok) ok=storage_->removePath(journal);
  *h={};
  return ok || fail("io_error");
}
bool AppStorageService::stat(const char* path,uint32_t& length) {
  int token=open(path,"r"); if (!token) return false;
  bool ok=size(token,length); close(token,false); return ok;
}
bool AppStorageService::list(const char* path,uint16_t page,StorageEntry* entries,uint8_t& count) {
  char target[129]; if (!resolve(path,target,false)) return false;
  uint16_t total=0;
  if (page>16383) return fail("invalid_offset");
  if (!storage_->listDirectoryPage(target,page*4,entries,4,count,total)) return fail("io_error");
  return true;
}
bool AppStorageService::mkdir(const char* path) {
  char target[129]; if (!resolve(path,target,true)) return false;
  char root[80]; snprintf(root,sizeof(root),"/OSEsp32/Data/%s",package_.manifest.appId);
  if (!storage_->makeDirectory(root)) return fail("io_error");
  return storage_->makeDirectory(target) || fail("io_error");
}
