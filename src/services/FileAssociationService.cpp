#include "FileAssociationService.h"
#include <strings.h>
void FileAssociationService::begin(StorageService& storage,YapPackageService& packages) {
  storage_=&storage; packages_=&packages;
}
void FileAssociationService::scan(const char* document) {
  count_=0; offset_=0; truncated_=false; scanning_=false; extension_[0]=0;
  const char* ext=strrchr(document,'.');
  if (!ext || strlen(ext+1)>7 || !ext[1]) return;
  strlcpy(extension_,ext+1,sizeof(extension_));
  for (char* p=extension_;*p;++p) {
    if (*p>='A' && *p<='Z') *p+=32;
    if (!((*p>='a' && *p<='z') || (*p>='0' && *p<='9'))) { extension_[0]=0; return; }
  }
  if (StorageService::isImagePath(document)) {
    strlcpy(candidates_[0].name,"Image Viewer",49);
    strlcpy(candidates_[0].path,"@viewer",129); count_=1;
  }
  generation_=storage_->generation(); scanning_=true;
}
void FileAssociationService::update() {
  if (!scanning_) return;
  if (!storage_->mounted() || generation_!=storage_->generation()) { scanning_=false; count_=0; return; }
  StorageEntry entry; uint8_t count=0; uint16_t total=0;
  if (!storage_->listDirectoryPage("/OSEsp32/Apps",offset_,&entry,1,count,total) || !count) { scanning_=false; return; }
  ++offset_;
  if (!entry.directory && StorageService::isYapPath(entry.path)) {
    YapPackageInfo package;
    if (packages_->inspect(entry.path,package)==YapError::None && (package.manifest.capabilities&YapDocumentsOpen)) {
      for (uint8_t i=0;i<package.manifest.associationCount;++i) {
        if (strcasecmp(extension_,package.manifest.associations[i])) continue;
        if (count_==MAX_CANDIDATES) truncated_=true;
        else {
          strlcpy(candidates_[count_].path,entry.path,129);
          strlcpy(candidates_[count_++].name,package.manifest.name,49);
        }
        break;
      }
    }
  }
  if (offset_>=total || offset_>=64) { scanning_=false; if (offset_<total) truncated_=true; }
}
int FileAssociationService::defaultIndex() {
  if (!*extension_) return -1;
  Preferences prefs;
  if (!prefs.begin("osesp32_assoc",true)) return -1;
  char path[129]={}; prefs.getString(extension_,path,sizeof(path)); prefs.end();
  for (uint8_t i=0;i<count_;++i) if (!strcmp(path,candidates_[i].path)) return i;
  return -1;
}
bool FileAssociationService::remember(uint8_t index) {
  if (index>=count_ || !*extension_) return false;
  Preferences prefs; if (!prefs.begin("osesp32_assoc",false)) return false;
  bool ok=prefs.putString(extension_,candidates_[index].path)>0; prefs.end(); return ok;
}
bool FileAssociationService::reset() {
  Preferences prefs; if (!prefs.begin("osesp32_assoc",false)) return false;
  bool ok=prefs.clear(); prefs.end(); return ok;
}
