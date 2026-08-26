#pragma once

#include "SystemSettingsService.h"

class LocalizationService {
 public:
  void setLanguage(SystemLanguage language) { language_ = language; }
  SystemLanguage language() const { return language_; }
  bool russian() const { return language_ == SystemLanguage::Russian; }
  const char* text(const char* english, const char* russian) const {
    return this->russian() ? russian : english;
  }

 private:
  SystemLanguage language_ = SystemLanguage::English;
};
