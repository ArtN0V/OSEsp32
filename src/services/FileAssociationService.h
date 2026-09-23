#pragma once
#include "YapPackageService.h"
#include <Preferences.h>

class FileAssociationService {
 public:
  struct Candidate { char name[49]={}; char path[129]={}; };
  static constexpr uint8_t MAX_CANDIDATES=8;
  void begin(StorageService& storage,YapPackageService& packages);
  void scan(const char* document);
  void update(); // One package per loop; no unbounded registry allocation.
  bool scanning() const { return scanning_; }
  uint8_t count() const { return count_; }
  bool truncated() const { return truncated_; }
  const Candidate& candidate(uint8_t index) const { return candidates_[index]; }
  int defaultIndex();
  bool remember(uint8_t index);
  bool reset();
 private:
  StorageService* storage_=nullptr;
  YapPackageService* packages_=nullptr;
  Candidate candidates_[MAX_CANDIDATES];
  uint8_t count_=0;
  uint16_t offset_=0;
  uint32_t generation_=0;
  bool scanning_=false,truncated_=false;
  char extension_[8]={};
};
