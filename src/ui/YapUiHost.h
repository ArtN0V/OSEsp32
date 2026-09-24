#pragma once
#include "SystemKeyboard.h"
#include "../runtime/YapRuntimeService.h"

// Single system owner for app widgets, text entry and exact-file pickers.
// LVGL callbacks only queue actions; update() performs all teardown/I/O.
class YapUiHost {
 public:
  // richUi/viewport come from the package being prepared. The runtime still
  // contains the previous package until start(), so it is not authoritative
  // while this object tree is created.
  void begin(lv_obj_t* parent, YapRuntimeService& runtime, StorageService& storage,
             SystemKeyboard& keyboard, const lv_font_t* font, bool russian,
             int top, bool richUi, uint16_t viewportWidth,
             uint16_t viewportHeight);
  void update();
  void shutdown();
  void storageLost();
  bool takeRetry();
  bool takeClose();
  void storageRestored();
  void retryFailed();
 private:
  YapRuntimeService* runtime_=nullptr;
  StorageService* storage_=nullptr;
  SystemKeyboard* keyboard_=nullptr;
  const lv_font_t* font_=nullptr;
  bool russian_=false, richUi_=false, lost_=false, retry_=false, close_=false;
  lv_obj_t* buttons_[6]={};
  lv_obj_t* widgetRoot_=nullptr;
  lv_obj_t* widgetObjects_[YapRuntimeService::MAX_UI_WIDGETS]={};
  YapRuntimeService::UiKind widgetKinds_[YapRuntimeService::MAX_UI_WIDGETS]={};
  uint8_t widgetRows_[YapRuntimeService::MAX_UI_WIDGETS]={};
  uint8_t hostedWidgetCount_=0;
  lv_obj_t* modal_=nullptr;
  lv_obj_t* textarea_=nullptr;
  lv_obj_t* message_=nullptr;
  LvglTextareaInputClient input_;
  uint32_t version_=UINT32_MAX;
  int action_=0;
  uint8_t fieldId_=0;
  YapRuntimeService::Request shown_=YapRuntimeService::Request::None;
  char directory_[129]="/Documents";
  char selected_[129]={};
  uint16_t page_=0, total_=0;
  StorageEntry entries_[4];
  uint8_t count_=0;
  const char* tr(const char* en,const char* ru) const { return russian_ ? ru : en; }
  lv_obj_t* button(lv_obj_t* parent,const char* text,int x,int y,int w,int action);
  void newModal(const char* title);
  void dismiss();
  void picker();
  void error(const char* text);
  void choose(const char* mode);
  void rebuildWidgets();
  const YapRuntimeService::UiWidget* widgetById(uint8_t id) const;
  void openTextField(uint8_t id);
  static void event(lv_event_t* event);
  static void richEvent(lv_event_t* event);
  static void textEvent(lv_event_t* event);
};
