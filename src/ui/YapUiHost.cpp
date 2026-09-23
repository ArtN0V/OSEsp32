#include "YapUiHost.h"
#include <strings.h>

lv_obj_t* YapUiHost::button(lv_obj_t* parent,const char* text,int x,int y,int w,int action) {
  auto* b=lv_button_create(parent);
  lv_obj_set_pos(b,x,y); lv_obj_set_size(b,w,28);
  lv_obj_set_user_data(b,reinterpret_cast<void*>(static_cast<intptr_t>(action)));
  lv_obj_add_event_cb(b,event,LV_EVENT_CLICKED,this);
  auto* label=lv_label_create(b); lv_label_set_text(label,text);
  lv_obj_set_width(label,w-8); lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);
  lv_obj_center(label); return b;
}
void YapUiHost::event(lv_event_t* event) {
  auto* self=static_cast<YapUiHost*>(lv_event_get_user_data(event));
  int action=static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(event))));
  if (action>=1 && action<=6) self->runtime_->postEvent(action);
  else self->action_=action;
}
void YapUiHost::textEvent(lv_event_t* event) {
  auto* self=static_cast<YapUiHost*>(lv_event_get_user_data(event));
  self->action_=lv_event_get_code(event)==LV_EVENT_READY ? 11 : 12;
}
void YapUiHost::begin(lv_obj_t* parent,YapRuntimeService& runtime,StorageService& storage,
                      SystemKeyboard& keyboard,const lv_font_t* font,bool russian,int top) {
  shutdown(); runtime_=&runtime; storage_=&storage; keyboard_=&keyboard;
  font_=font; russian_=russian; version_=UINT32_MAX; lost_=retry_=close_=false;
  for (int i=0;i<6;++i) {
    buttons_[i]=button(parent,"",8+(i%2)*150,top+(i/2)*32,144,i+1);
    lv_obj_add_flag(buttons_[i],LV_OBJ_FLAG_HIDDEN);
  }
}
void YapUiHost::dismiss() {
  if (keyboard_) keyboard_->hide();
  input_.setTarget(nullptr);
  if (modal_) lv_obj_delete(modal_);
  modal_=textarea_=message_=nullptr; shown_=YapRuntimeService::Request::None;
  action_=0;
}
void YapUiHost::shutdown() {
  dismiss();
  for (auto& b:buttons_) { if (b) lv_obj_delete(b); b=nullptr; }
  runtime_=nullptr; keyboard_=nullptr;
}
void YapUiHost::newModal(const char* title) {
  dismiss(); modal_=lv_obj_create(lv_layer_top());
  lv_obj_set_pos(modal_,0,0); lv_obj_set_size(modal_,320,240);
  lv_obj_set_style_pad_all(modal_,0,0); lv_obj_set_style_radius(modal_,0,0);
  lv_obj_set_style_text_font(modal_,font_,0);
  lv_obj_remove_flag(modal_,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(modal_,lv_color_hex(0xE9EEF5),0);
  message_=lv_label_create(modal_); lv_obj_set_pos(message_,8,5);
  lv_obj_set_width(message_,304); lv_label_set_long_mode(message_,LV_LABEL_LONG_DOT);
  lv_label_set_text(message_,title);
}
void YapUiHost::error(const char* text) { if (message_) lv_label_set_text(message_,text); }
void YapUiHost::picker() {
  auto request=runtime_->request();
  char title[180];
  snprintf(title,sizeof(title),"%s: %s",request==YapRuntimeService::Request::Save ? runtime_->requestText() : tr("Open","Открыть"),directory_);
  newModal(title); shown_=request;
  count_=0; total_=0;
  if (!storage_->listDirectoryPage(directory_,page_*4,entries_,4,count_,total_)) error("io_error");
  for (uint8_t i=0;i<count_;++i) {
    char text[60]; snprintf(text,sizeof(text),"%s%s",entries_[i].directory ? LV_SYMBOL_DIRECTORY " " : "",entries_[i].name);
    auto* b=button(modal_,text,8,30+i*33,304,20+i);
    bool allowed=entries_[i].directory ? strcasecmp(entries_[i].path,"/OSEsp32")!=0 :
      runtime_->files().permitsDocument(entries_[i].path,request==YapRuntimeService::Request::Open ? "r" : "w");
    if (!allowed) lv_obj_add_state(b,LV_STATE_DISABLED);
  }
  button(modal_,LV_SYMBOL_UP,8,164,48,30);
  auto* prev=button(modal_,"<",64,164,48,31);
  auto* next=button(modal_,">",120,164,48,32);
  if (!page_) lv_obj_add_state(prev,LV_STATE_DISABLED);
  if ((page_+1)*4>=total_) lv_obj_add_state(next,LV_STATE_DISABLED);
  button(modal_,tr("Cancel","Отмена"),8,204,112,10);
  if (request==YapRuntimeService::Request::Save)
    button(modal_,tr("Create new","Создать"),160,204,152,33);
}
void YapUiHost::choose(const char* mode) {
  int handle=runtime_->files().grant(selected_,mode);
  if (!handle) { error(runtime_->files().error()); return; }
  runtime_->reply(nullptr,handle); dismiss();
}
void YapUiHost::storageLost() {
  if (lost_) return;
  lost_=true; runtime_->pauseStorage();
  newModal(tr("SD removed. Unsaved app state is paused.","SD извлечена. Приложение приостановлено."));
  button(modal_,tr("Retry","Повторить"),10,80,144,40);
  button(modal_,tr("Close app","Закрыть"),166,80,144,41);
}
bool YapUiHost::takeRetry() { bool value=retry_; retry_=false; return value; }
bool YapUiHost::takeClose() { bool value=close_; close_=false; return value; }
void YapUiHost::storageRestored() { dismiss(); lost_=false; runtime_->resumeStorage(); }
void YapUiHost::retryFailed() { error(tr("Insert the original app card and retry","Верните карту приложения и повторите")); }
void YapUiHost::update() {
  if (!runtime_) return;
  int action=action_; action_=0;
  if (lost_) {
    if (action==40) retry_=true;
    if (action==41) close_=true;
    return;
  }
  if (version_!=runtime_->uiVersion()) {
    version_=runtime_->uiVersion();
    for (int i=0;i<6;++i) {
      const char* text=runtime_->buttons()[i].text;
      lv_label_set_text(lv_obj_get_child(buttons_[i],0),text);
      if (*text) lv_obj_remove_flag(buttons_[i],LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(buttons_[i],LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (action==10) { runtime_->reply(nullptr,0,"cancelled"); dismiss(); return; }
  if (action==11 && textarea_) {
    const char* text=lv_textarea_get_text(textarea_);
    if (strlen(text)>192) { error("text_too_long"); return; }
    runtime_->reply(text); dismiss(); return;
  }
  if (action==12 && textarea_) {
    keyboard_->show(input_,russian_ ? KeyboardLanguage::Russian : KeyboardLanguage::English);
    return;
  }
  if (action==34) { choose("w"); return; }
  if (action>=20 && action<=23 && action-20<count_) {
    const auto& entry=entries_[action-20];
    if (entry.directory) { strlcpy(directory_,entry.path,sizeof(directory_)); page_=0; picker(); }
    else {
      strlcpy(selected_,entry.path,sizeof(selected_));
      if (shown_==YapRuntimeService::Request::Open) choose("r");
      else {
        newModal(tr("Replace selected file?","Заменить выбранный файл?"));
        shown_=YapRuntimeService::Request::Save;
        auto* name=lv_label_create(modal_); lv_obj_set_pos(name,8,40); lv_obj_set_width(name,304); lv_label_set_text(name,selected_);
        button(modal_,tr("Replace","Заменить"),8,100,144,34);
        button(modal_,tr("Cancel","Отмена"),164,100,144,10);
      }
    }
    return;
  }
  if (action==30) {
    char* slash=strrchr(directory_,'/'); if (slash==directory_) directory_[1]=0; else if (slash) *slash=0;
    page_=0; picker(); return;
  }
  if (action==31 || action==32) {
    if (action==31 && page_) --page_;
    if (action==32 && static_cast<uint32_t>(page_+1)*4<total_) ++page_;
    picker(); return;
  }
  if (action==33) {
    int n=snprintf(selected_,sizeof(selected_),"%s%s%s",directory_,strcmp(directory_,"/") ? "/" : "",runtime_->requestText());
    if (n<0 || n>=static_cast<int>(sizeof(selected_))) { error("invalid_path"); return; }
    choose("x"); return;
  }
  auto request=runtime_->request();
  if (modal_ || request==YapRuntimeService::Request::None || request==YapRuntimeService::Request::Event) return;
  if (request==YapRuntimeService::Request::Text) {
    newModal(tr("Text input","Ввод текста")); shown_=request;
    textarea_=lv_textarea_create(modal_); lv_obj_set_pos(textarea_,6,24); lv_obj_set_size(textarea_,308,55);
    lv_obj_add_event_cb(textarea_,textEvent,LV_EVENT_READY,this);
    lv_obj_add_event_cb(textarea_,textEvent,LV_EVENT_CLICKED,this);
    lv_textarea_set_max_length(textarea_,64); lv_textarea_set_text(textarea_,runtime_->requestText());
    button(modal_,"OK",8,86,144,11); button(modal_,tr("Cancel","Отмена"),168,86,144,10);
    input_.setTarget(textarea_);
    if (!keyboard_->show(input_,russian_ ? KeyboardLanguage::Russian : KeyboardLanguage::English)) {
      runtime_->reply(nullptr,0,"out_of_memory"); dismiss();
    }
  } else { page_=0; strlcpy(directory_,"/Documents",sizeof(directory_)); picker(); }
}
