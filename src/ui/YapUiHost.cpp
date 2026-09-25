#include "YapUiHost.h"
#include <strings.h>
#include <esp_heap_caps.h>
#include "../kernel/ResetDiagnostics.h"

namespace {
constexpr uint32_t CANVAS_COLORS[16]={
  0x101820,0xE74C3C,0x2ECC71,0x3498DB,0xF1C40F,0x9B59B6,0x1ABC9C,0xECF0F1,
  0x7F8C8D,0xC0392B,0x27AE60,0x2980B9,0xF39C12,0x8E44AD,0x16A085,0xFFFFFF};

uint16_t readLe16(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) | static_cast<uint16_t>(data[1])<<8;
}
uint32_t readLe32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) | static_cast<uint32_t>(data[1])<<8 |
         static_cast<uint32_t>(data[2])<<16 | static_cast<uint32_t>(data[3])<<24;
}
void writeLe16(uint8_t* data,uint16_t value) {
  data[0]=value&0xff; data[1]=value>>8;
}
void writeLe32(uint8_t* data,uint32_t value) {
  data[0]=value&0xff; data[1]=(value>>8)&0xff;
  data[2]=(value>>16)&0xff; data[3]=(value>>24)&0xff;
}
bool validMask(uint32_t mask) {
  if (!mask) return false;
  while (!(mask&1)) mask>>=1;
  while (mask&1) mask>>=1;
  return mask==0;
}
uint8_t maskChannel(uint32_t pixel,uint32_t mask) {
  uint8_t shift=0, bits=0;
  while (!(mask&1)) { mask>>=1; ++shift; }
  while (mask&1) { mask>>=1; ++bits; }
  const uint32_t maximum=bits==32 ? UINT32_MAX : ((1UL<<bits)-1);
  const uint32_t value=(pixel>>shift)&maximum;
  return static_cast<uint8_t>((static_cast<uint64_t>(value)*255+maximum/2)/maximum);
}
uint8_t nearestCanvasColor(uint8_t red,uint8_t green,uint8_t blue) {
  uint8_t nearest=0; uint32_t best=UINT32_MAX;
  for (uint8_t index=0;index<16;++index) {
    const uint32_t color=CANVAS_COLORS[index];
    const int dr=red-static_cast<int>(color>>16);
    const int dg=green-static_cast<int>((color>>8)&0xff);
    const int db=blue-static_cast<int>(color&0xff);
    const uint32_t distance=dr*dr+dg*dg+db*db;
    if (distance<best) { best=distance; nearest=index; }
  }
  return nearest;
}
}

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
const YapRuntimeService::UiWidget* YapUiHost::widgetById(uint8_t id) const {
  if (!runtime_) return nullptr;
  for (uint8_t index=0;index<runtime_->widgetCount();++index)
    if (runtime_->widgets()[index].id==id) return &runtime_->widgets()[index];
  return nullptr;
}
void YapUiHost::richEvent(lv_event_t* event) {
  auto* self=static_cast<YapUiHost*>(lv_event_get_user_data(event));
  const intptr_t tag=reinterpret_cast<intptr_t>(lv_obj_get_user_data(lv_event_get_target_obj(event)));
  const uint8_t id=tag&0xff; const uint8_t row=(tag>>8)&0xff;
  const auto* item=self->widgetById(id); if (!item) return;
  const lv_event_code_t code=lv_event_get_code(event);
  if (code==LV_EVENT_GESTURE) {
    YapRuntimeService::UiEventKind kind;
    switch (lv_indev_get_gesture_dir(lv_indev_active())) {
      case LV_DIR_LEFT: kind=YapRuntimeService::UiEventKind::SwipeLeft; break;
      case LV_DIR_RIGHT: kind=YapRuntimeService::UiEventKind::SwipeRight; break;
      case LV_DIR_TOP: kind=YapRuntimeService::UiEventKind::SwipeUp; break;
      default: kind=YapRuntimeService::UiEventKind::SwipeDown; break;
    }
    self->runtime_->postUiEvent(id,kind); return;
  }
  if (row && code==LV_EVENT_SHORT_CLICKED) {
    self->runtime_->postUiEvent(id,YapRuntimeService::UiEventKind::Change,row); return;
  }
  if (item->kind==YapRuntimeService::UiKind::TextField && code==LV_EVENT_CLICKED) {
    self->fieldId_=id; self->action_=50; return;
  }
  if (item->kind==YapRuntimeService::UiKind::Toggle && code==LV_EVENT_VALUE_CHANGED) {
    const bool checked=lv_obj_has_state(lv_event_get_target_obj(event),LV_STATE_CHECKED);
    self->runtime_->postUiEvent(id,YapRuntimeService::UiEventKind::Change,checked); return;
  }
  if (code==LV_EVENT_LONG_PRESSED)
    self->runtime_->postUiEvent(id,YapRuntimeService::UiEventKind::Hold);
  else if (code==LV_EVENT_SHORT_CLICKED)
    self->runtime_->postUiEvent(id,YapRuntimeService::UiEventKind::Tap);
}
void YapUiHost::textEvent(lv_event_t* event) {
  auto* self=static_cast<YapUiHost*>(lv_event_get_user_data(event));
  self->action_=lv_event_get_code(event)==LV_EVENT_READY ? 11 : 12;
}
void YapUiHost::begin(lv_obj_t* parent,YapRuntimeService& runtime,StorageService& storage,
                      SystemKeyboard& keyboard,const lv_font_t* font,bool russian,int top,
                      bool richUi,uint16_t viewportWidth,uint16_t viewportHeight) {
  shutdown(); runtime_=&runtime; storage_=&storage; keyboard_=&keyboard;
  font_=font; russian_=russian; richUi_=richUi; version_=UINT32_MAX;
  lost_=retry_=close_=false;
  fieldId_=0;
  if (richUi_) {
    widgetRoot_=lv_obj_create(parent);
    const bool windowed=viewportWidth==300;
    lv_obj_set_pos(widgetRoot_,windowed ? 5 : 0,windowed ? 40 : 0);
    lv_obj_set_size(widgetRoot_,viewportWidth,viewportHeight);
    lv_obj_set_style_pad_all(widgetRoot_,0,0); lv_obj_set_style_border_width(widgetRoot_,0,0);
    lv_obj_set_style_radius(widgetRoot_,0,0); lv_obj_set_style_bg_opa(widgetRoot_,LV_OPA_TRANSP,0);
    lv_obj_remove_flag(widgetRoot_,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(widgetRoot_,LV_OBJ_FLAG_CLICKABLE);
    return;
  }
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
  action_=0; fieldId_=0;
}
void YapUiHost::shutdown() {
  dismiss();
  releaseCanvas(); canvasRequestStarted_=false;
  for (auto& b:buttons_) { if (b) lv_obj_delete(b); b=nullptr; }
  if (widgetRoot_) lv_obj_delete(widgetRoot_);
  widgetRoot_=nullptr; hostedWidgetCount_=0;
  for (uint8_t index=0;index<YapRuntimeService::MAX_UI_WIDGETS;++index) {
    widgetObjects_[index]=nullptr; widgetKinds_[index]=YapRuntimeService::UiKind::None;
    widgetRows_[index]=0;
  }
  runtime_=nullptr; keyboard_=nullptr; richUi_=false;
}

void YapUiHost::releaseCanvas() {
  cancelCanvasIo();
  canvasProbeActive_=false;
  const bool hadCanvas=canvas_ || canvasBuffer_;
  if (hadCanvas) ResetDiagnostics::mark(ResetCheckpoint::CanvasRelease);
  // The image cache is disabled in this build. Detach the variable source
  // before destroying its storage so no pending invalidation can inspect a
  // draw-buffer descriptor that has already been freed.
  if (canvas_) {
    lv_image_set_src(canvas_,nullptr);
    lv_obj_delete(canvas_);
  }
  canvas_=nullptr;
  if (canvasBuffer_) lv_draw_buf_destroy(canvasBuffer_);
  canvasBuffer_=nullptr; canvasFormat_=LV_COLOR_FORMAT_UNKNOWN;
  canvasWidth_=canvasHeight_=0;
  canvasFramesIssued_=canvasFramesRecorded_=0;
  canvasPreviousX_=canvasPreviousY_=-1;
  canvasTouchX_=canvasTouchY_=-1;
  if (hadCanvas) ResetDiagnostics::mark(ResetCheckpoint::CanvasReleased);
}

void YapUiHost::setCanvasPixel(int16_t x,int16_t y,uint16_t frame,bool overlay) {
  if (!canvasBuffer_ || x<0 || y<0 || x>=canvasWidth_ || y>=canvasHeight_) return;
  uint8_t* pixel=static_cast<uint8_t*>(lv_draw_buf_goto_xy(canvasBuffer_,x,y));
  if (!pixel) return;
  if (canvasFormat_==LV_COLOR_FORMAT_RGB565) {
    uint8_t red=overlay ? static_cast<uint8_t>(255-(frame*7)%128) : x*255/319;
    uint8_t green=overlay ? static_cast<uint8_t>((frame*29)%256) : y*255/203;
    uint8_t blue=overlay ? 255 : static_cast<uint8_t>(((x/20+y/20)&1) ? 190 : 35);
    *reinterpret_cast<uint16_t*>(pixel)=lv_color_to_u16(lv_color_make(red,green,blue));
  } else if (canvasFormat_==LV_COLOR_FORMAT_I8) {
    *pixel=overlay ? static_cast<uint8_t>(160+(frame*13)%96)
                   : static_cast<uint8_t>((x+y*2)&0xff);
  } else {
    const uint8_t value=overlay ? static_cast<uint8_t>((frame%15)+1)
                                : static_cast<uint8_t>((x/20+y/17)&0x0f);
    if (x&1) *pixel=(*pixel&0xf0)|value;
    else *pixel=(*pixel&0x0f)|(value<<4);
  }
}

void YapUiHost::setCanvasIndexPixel(int16_t x,int16_t y,uint8_t color) {
  if (!canvasBuffer_ || canvasFormat_!=LV_COLOR_FORMAT_I4 || x<0 || y<0 ||
      x>=canvasWidth_ || y>=canvasHeight_) return;
  uint8_t* pixel=static_cast<uint8_t*>(lv_draw_buf_goto_xy(canvasBuffer_,x,y));
  if (!pixel) return;
  color&=0x0f;
  if (x&1) *pixel=(*pixel&0xf0)|color;
  else *pixel=(*pixel&0x0f)|(color<<4);
}

void YapUiHost::paintCanvasRect(int16_t x,int16_t y,int16_t width,int16_t height,
                                uint16_t frame,bool overlay) {
  for (int16_t row=y;row<y+height;++row)
    for (int16_t column=x;column<x+width;++column)
      setCanvasPixel(column,row,frame,overlay);
}

void YapUiHost::beginCanvasProbe(const char* format) {
  releaseCanvas(); canvasStats_={};
  strlcpy(canvasStats_.format,format,sizeof(canvasStats_.format));
  if (!strcmp(format,"rgb565")) canvasFormat_=LV_COLOR_FORMAT_RGB565;
  else if (!strcmp(format,"i8")) canvasFormat_=LV_COLOR_FORMAT_I8;
  else canvasFormat_=LV_COLOR_FORMAT_I4;
  canvasStats_.freeBefore=ESP.getFreeHeap();
  canvasStats_.largestBefore=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  ResetDiagnostics::mark(ResetCheckpoint::CanvasAllocate);
  const uint32_t allocationStarted=micros();
  canvas_=lv_image_create(lv_obj_get_parent(widgetRoot_));
  canvasBuffer_=lv_draw_buf_create(320,204,canvasFormat_,LV_STRIDE_AUTO);
  canvasWidth_=320; canvasHeight_=204;
  canvasStats_.allocationUs=micros()-allocationStarted;
  if (!canvas_ || !canvasBuffer_) {
    releaseCanvas();
    canvasStats_.freeActive=ESP.getFreeHeap();
    canvasStats_.largestActive=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    canvasStats_.minimumFree=canvasStats_.freeActive;
    runtime_->replyCanvas(canvasStats_,"out_of_memory"); return;
  }
  lv_image_set_src(canvas_,canvasBuffer_);
  lv_obj_set_pos(canvas_,0,0); lv_obj_set_size(canvas_,320,204);
  lv_obj_move_to_index(canvas_,0);
  if (canvasFormat_==LV_COLOR_FORMAT_I8) {
    for (uint16_t index=0;index<256;++index) {
      const uint8_t red=((index>>5)&7)*255/7;
      const uint8_t green=((index>>2)&7)*255/7;
      const uint8_t blue=(index&3)*255/3;
      lv_draw_buf_set_palette(canvasBuffer_,index,lv_color32_make(red,green,blue,255));
    }
  } else if (canvasFormat_==LV_COLOR_FORMAT_I4) {
    for (uint8_t index=0;index<16;++index) {
      const uint32_t color=CANVAS_COLORS[index];
      lv_draw_buf_set_palette(canvasBuffer_,index,lv_color32_make(
          color>>16,(color>>8)&0xff,color&0xff,255));
    }
  }
  ResetDiagnostics::mark(ResetCheckpoint::CanvasFill);
  const uint32_t fillStarted=micros();
  paintCanvasRect(0,0,320,204,0,false);
  lv_draw_buf_flush_cache(canvasBuffer_,nullptr);
  canvasStats_.fillUs=micros()-fillStarted;
  canvasStats_.bufferBytes=canvasBuffer_->data_size;
  canvasStats_.freeActive=ESP.getFreeHeap();
  canvasStats_.largestActive=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  canvasStats_.minimumFree=canvasStats_.freeActive;
  lv_obj_invalidate(canvas_);
  ResetDiagnostics::mark(ResetCheckpoint::CanvasAnimate);
  canvasProbeActive_=true; canvasRequestStarted_=true;
  canvasFramesIssued_=canvasFramesRecorded_=0;
  canvasFrameTotalMs_=0; canvasStats_.maximumFrameMs=0;
  canvasPreviousX_=canvasPreviousY_=-1; canvasLastFrameMs_=millis();
}

bool YapUiHost::createDrawingCanvas(uint16_t width,uint16_t height) {
  releaseCanvas();
  canvasFormat_=LV_COLOR_FORMAT_I4;
  canvasWidth_=width; canvasHeight_=height;
  ResetDiagnostics::mark(ResetCheckpoint::CanvasAllocate);
  canvas_=lv_image_create(lv_obj_get_parent(widgetRoot_));
  canvasBuffer_=lv_draw_buf_create(width,height,canvasFormat_,LV_STRIDE_AUTO);
  if (!canvas_ || !canvasBuffer_) { releaseCanvas(); return false; }
  for (uint8_t index=0;index<16;++index) {
    const uint32_t color=CANVAS_COLORS[index];
    lv_draw_buf_set_palette(canvasBuffer_,index,lv_color32_make(
        color>>16,(color>>8)&0xff,color&0xff,255));
  }
  lv_image_set_src(canvas_,canvasBuffer_);
  lv_obj_set_pos(canvas_,0,0); lv_obj_set_size(canvas_,width,height);
  lv_obj_move_to_index(canvas_,0);
  lv_obj_add_flag(canvas_,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(canvas_,LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_add_event_cb(canvas_,canvasEvent,LV_EVENT_ALL,this);
  clearDrawingCanvas(15);
  ResetDiagnostics::mark(ResetCheckpoint::CanvasAnimate);
  return true;
}

void YapUiHost::clearDrawingCanvas(uint8_t color) {
  if (!canvas_ || !canvasBuffer_ || canvasFormat_!=LV_COLOR_FORMAT_I4) return;
  uint8_t* pixels=static_cast<uint8_t*>(lv_draw_buf_goto_xy(canvasBuffer_,0,0));
  if (!pixels) return;
  memset(pixels,static_cast<uint8_t>((color<<4)|color),
         canvasBuffer_->header.stride*canvasHeight_);
  lv_draw_buf_flush_cache(canvasBuffer_,nullptr);
  lv_obj_invalidate(canvas_);
}

void YapUiHost::drawCanvasLine(const YapRuntimeService::CanvasCommand& command) {
  if (!canvas_ || !canvasBuffer_ || canvasFormat_!=LV_COLOR_FORMAT_I4) return;
  int x0=command.x1, y0=command.y1, x1=command.x2, y1=command.y2;
  const int half=command.thickness/2;
  const int dx=abs(x1-x0), stepX=x0<x1 ? 1 : -1;
  const int dy=-abs(y1-y0), stepY=y0<y1 ? 1 : -1;
  int error=dx+dy;
  while (true) {
    for (int y=y0-half;y<=y0+half;++y)
      for (int x=x0-half;x<=x0+half;++x)
        setCanvasIndexPixel(x,y,command.color);
    if (x0==x1 && y0==y1) break;
    const int doubled=2*error;
    if (doubled>=dy) { error+=dy; x0+=stepX; }
    if (doubled<=dx) { error+=dx; y0+=stepY; }
  }
  lv_draw_buf_flush_cache(canvasBuffer_,nullptr);
  lv_area_t objectArea; lv_obj_get_coords(canvas_,&objectArea);
  const int left=max(0,min(command.x1,command.x2)-half);
  const int top=max(0,min(command.y1,command.y2)-half);
  const int right=min(static_cast<int>(canvasWidth_)-1,
                      max(command.x1,command.x2)+half);
  const int bottom=min(static_cast<int>(canvasHeight_)-1,
                       max(command.y1,command.y2)+half);
  lv_area_t dirty={
    static_cast<int32_t>(objectArea.x1+left),
    static_cast<int32_t>(objectArea.y1+top),
    static_cast<int32_t>(objectArea.x1+right),
    static_cast<int32_t>(objectArea.y1+bottom)};
  lv_obj_invalidate_area(canvas_,&dirty);
}

uint8_t YapUiHost::canvasIndexPixel(int16_t x,int16_t y) const {
  if (!canvasBuffer_ || canvasFormat_!=LV_COLOR_FORMAT_I4 || x<0 || y<0 ||
      x>=canvasWidth_ || y>=canvasHeight_) return 0;
  const uint8_t* pixel=static_cast<const uint8_t*>(
      lv_draw_buf_goto_xy(canvasBuffer_,x,y));
  if (!pixel) return 0;
  return x&1 ? *pixel&0x0f : *pixel>>4;
}

bool YapUiHost::beginCanvasBmpLoad(int handle,const char*& errorText) {
  errorText=nullptr;
  if (!canvasBuffer_ || canvasFormat_!=LV_COLOR_FORMAT_I4) {
    errorText="no_canvas"; return false;
  }
  uint32_t length=0;
  if (!runtime_->files().size(handle,length)) {
    errorText=runtime_->files().error(); return false;
  }
  if (length<54) { errorText="invalid_bmp"; return false; }
  const size_t headerSize=min(static_cast<uint32_t>(sizeof(canvasIoBuffer_)),
                              min(length,static_cast<uint32_t>(70)));
  if (!runtime_->files().seek(handle,0)) {
    errorText=runtime_->files().error(); return false;
  }
  size_t read=0;
  if (!runtime_->files().read(handle,canvasIoBuffer_,headerSize,read) || read!=headerSize) {
    errorText=runtime_->files().error(); return false;
  }
  const uint32_t pixelOffset=readLe32(canvasIoBuffer_+10);
  const uint32_t dibSize=readLe32(canvasIoBuffer_+14);
  const int32_t signedWidth=static_cast<int32_t>(readLe32(canvasIoBuffer_+18));
  const int32_t signedHeight=static_cast<int32_t>(readLe32(canvasIoBuffer_+22));
  const uint16_t bits=readLe16(canvasIoBuffer_+28);
  const uint32_t compression=readLe32(canvasIoBuffer_+30);
  if (canvasIoBuffer_[0]!='B' || canvasIoBuffer_[1]!='M' || dibSize<40 ||
      readLe16(canvasIoBuffer_+26)!=1 || signedWidth<=0 || signedHeight==0 ||
      signedHeight==INT32_MIN || (bits!=16 && bits!=24 && bits!=32) ||
      (bits==24 && compression!=0) ||
      ((bits==16 || bits==32) && compression!=0 && compression!=3)) {
    errorText="unsupported_bmp"; return false;
  }
  const uint32_t height=static_cast<uint32_t>(signedHeight<0 ? -signedHeight : signedHeight);
  const uint64_t rowStride=(static_cast<uint64_t>(signedWidth)*bits+31)/32*4;
  const uint64_t end=static_cast<uint64_t>(pixelOffset)+rowStride*height;
  if (static_cast<uint32_t>(signedWidth)>canvasWidth_ || height>canvasHeight_ ||
      rowStride>sizeof(canvasIoBuffer_) ||
      static_cast<uint64_t>(pixelOffset)<14ULL+dibSize || end>length) {
    errorText="bmp_too_large"; return false;
  }
  uint32_t redMask=0, greenMask=0, blueMask=0;
  if (compression==3) {
    if (headerSize<66 || pixelOffset<66) { errorText="invalid_bmp"; return false; }
    redMask=readLe32(canvasIoBuffer_+54);
    greenMask=readLe32(canvasIoBuffer_+58);
    blueMask=readLe32(canvasIoBuffer_+62);
    if (!validMask(redMask) || !validMask(greenMask) || !validMask(blueMask) ||
        (redMask&greenMask) || (redMask&blueMask) || (greenMask&blueMask)) {
      errorText="unsupported_bmp"; return false;
    }
  } else if (bits==16) {
    redMask=0x7c00; greenMask=0x03e0; blueMask=0x001f;
  } else if (bits==32) {
    redMask=0x00ff0000; greenMask=0x0000ff00; blueMask=0x000000ff;
  }
  clearDrawingCanvas(15);
  canvasIo_={}; canvasIo_.kind=CanvasIoKind::LoadBmp; canvasIo_.handle=handle;
  canvasIo_.fileSize=length; canvasIo_.pixelOffset=pixelOffset;
  canvasIo_.rowStride=static_cast<uint32_t>(rowStride);
  canvasIo_.redMask=redMask; canvasIo_.greenMask=greenMask; canvasIo_.blueMask=blueMask;
  canvasIo_.width=signedWidth; canvasIo_.height=height; canvasIo_.bitsPerPixel=bits;
  canvasIo_.topDown=signedHeight<0;
  return true;
}

bool YapUiHost::beginCanvasBmpSave(int handle,const char*& errorText) {
  errorText=nullptr;
  if (!canvasBuffer_ || canvasFormat_!=LV_COLOR_FORMAT_I4) {
    errorText="no_canvas"; return false;
  }
  const uint32_t rowStride=(static_cast<uint32_t>(canvasWidth_)*3+3)&~3UL;
  const uint32_t total=54+rowStride*canvasHeight_;
  if (rowStride>sizeof(canvasIoBuffer_) || total>AppStorageService::MAX_FILE_SIZE) {
    errorText="canvas_too_large"; return false;
  }
  if (!runtime_->files().seek(handle,0)) {
    errorText=runtime_->files().error(); return false;
  }
  memset(canvasIoBuffer_,0,54);
  canvasIoBuffer_[0]='B'; canvasIoBuffer_[1]='M';
  writeLe32(canvasIoBuffer_+2,total); writeLe32(canvasIoBuffer_+10,54);
  writeLe32(canvasIoBuffer_+14,40); writeLe32(canvasIoBuffer_+18,canvasWidth_);
  writeLe32(canvasIoBuffer_+22,canvasHeight_); writeLe16(canvasIoBuffer_+26,1);
  writeLe16(canvasIoBuffer_+28,24); writeLe32(canvasIoBuffer_+34,total-54);
  writeLe32(canvasIoBuffer_+38,2835); writeLe32(canvasIoBuffer_+42,2835);
  if (!runtime_->files().write(handle,canvasIoBuffer_,54)) {
    errorText=runtime_->files().error(); return false;
  }
  canvasIo_={}; canvasIo_.kind=CanvasIoKind::SaveBmp; canvasIo_.handle=handle;
  canvasIo_.fileSize=total; canvasIo_.pixelOffset=54; canvasIo_.rowStride=rowStride;
  canvasIo_.width=canvasWidth_; canvasIo_.height=canvasHeight_;
  canvasIo_.bitsPerPixel=24;
  return true;
}

void YapUiHost::cancelCanvasIo() { canvasIo_={}; }

void YapUiHost::updateCanvasIo() {
  if (canvasIo_.kind==CanvasIoKind::None || !runtime_) return;
  auto fail=[this](const char* errorText) {
    runtime_->replyCanvasCommand(errorText ? errorText : "io_error");
    cancelCanvasIo();
  };
  if (canvasIo_.row>=canvasIo_.height) {
    if (canvasIo_.kind==CanvasIoKind::SaveBmp &&
        !runtime_->files().flush(canvasIo_.handle)) {
      fail(runtime_->files().error()); return;
    }
    runtime_->replyCanvasCommand(); cancelCanvasIo(); return;
  }
  if (canvasIo_.kind==CanvasIoKind::LoadBmp) {
    const uint32_t offset=canvasIo_.pixelOffset+canvasIo_.row*canvasIo_.rowStride;
    if (!runtime_->files().seek(canvasIo_.handle,offset)) {
      fail(runtime_->files().error()); return;
    }
    size_t cursor=0;
    while (cursor<canvasIo_.rowStride) {
      const size_t wanted=min(static_cast<size_t>(512),
                              static_cast<size_t>(canvasIo_.rowStride-cursor));
      size_t count=0;
      if (!runtime_->files().read(canvasIo_.handle,canvasIoBuffer_+cursor,wanted,count) ||
          count!=wanted) { fail(runtime_->files().error()); return; }
      cursor+=count;
    }
    const int16_t targetY=canvasIo_.topDown ? canvasIo_.row
                                            : canvasIo_.height-1-canvasIo_.row;
    const uint8_t bytesPerPixel=canvasIo_.bitsPerPixel/8;
    for (uint16_t x=0;x<canvasIo_.width;++x) {
      const uint8_t* source=canvasIoBuffer_+x*bytesPerPixel;
      uint8_t red=0, green=0, blue=0;
      if (canvasIo_.bitsPerPixel==24) {
        blue=source[0]; green=source[1]; red=source[2];
      } else {
        const uint32_t pixel=canvasIo_.bitsPerPixel==16 ? readLe16(source)
                                                       : readLe32(source);
        red=maskChannel(pixel,canvasIo_.redMask);
        green=maskChannel(pixel,canvasIo_.greenMask);
        blue=maskChannel(pixel,canvasIo_.blueMask);
      }
      setCanvasIndexPixel(x,targetY,nearestCanvasColor(red,green,blue));
    }
    lv_draw_buf_flush_cache(canvasBuffer_,nullptr);
    lv_area_t area; lv_obj_get_coords(canvas_,&area);
    lv_area_t dirty={area.x1,static_cast<int32_t>(area.y1+targetY),
                     static_cast<int32_t>(area.x1+canvasIo_.width-1),
                     static_cast<int32_t>(area.y1+targetY)};
    lv_obj_invalidate_area(canvas_,&dirty);
  } else {
    memset(canvasIoBuffer_,0,canvasIo_.rowStride);
    const int16_t sourceY=canvasIo_.height-1-canvasIo_.row;
    for (uint16_t x=0;x<canvasIo_.width;++x) {
      const uint32_t color=CANVAS_COLORS[canvasIndexPixel(x,sourceY)];
      canvasIoBuffer_[x*3]=color&0xff;
      canvasIoBuffer_[x*3+1]=(color>>8)&0xff;
      canvasIoBuffer_[x*3+2]=color>>16;
    }
    size_t cursor=0;
    while (cursor<canvasIo_.rowStride) {
      const size_t count=min(static_cast<size_t>(512),
                             static_cast<size_t>(canvasIo_.rowStride-cursor));
      if (!runtime_->files().write(canvasIo_.handle,canvasIoBuffer_+cursor,count)) {
        fail(runtime_->files().error()); return;
      }
      cursor+=count;
    }
  }
  ++canvasIo_.row;
}

void YapUiHost::canvasEvent(lv_event_t* event) {
  auto* self=static_cast<YapUiHost*>(lv_event_get_user_data(event));
  if (!self || !self->runtime_ || !self->canvas_ ||
      self->canvasIo_.kind!=CanvasIoKind::None) return;
  const lv_event_code_t code=lv_event_get_code(event);
  if (code!=LV_EVENT_PRESSED && code!=LV_EVENT_PRESSING && code!=LV_EVENT_RELEASED)
    return;
  lv_indev_t* input=lv_indev_active();
  if (!input) return;
  lv_point_t point; lv_indev_get_point(input,&point);
  lv_area_t area; lv_obj_get_coords(self->canvas_,&area);
  int16_t x=point.x-area.x1, y=point.y-area.y1;
  x=constrain(x,0,static_cast<int16_t>(self->canvasWidth_-1));
  y=constrain(y,0,static_cast<int16_t>(self->canvasHeight_-1));
  if (code==LV_EVENT_PRESSING && x==self->canvasTouchX_ && y==self->canvasTouchY_) return;
  self->canvasTouchX_=x; self->canvasTouchY_=y;
  const auto kind=code==LV_EVENT_PRESSED ? YapRuntimeService::UiEventKind::CanvasDown
      : code==LV_EVENT_RELEASED ? YapRuntimeService::UiEventKind::CanvasUp
                               : YapRuntimeService::UiEventKind::CanvasMove;
  self->runtime_->postCanvasEvent(kind,x,y);
  if (code==LV_EVENT_RELEASED) self->canvasTouchX_=self->canvasTouchY_=-1;
}

void YapUiHost::updateCanvasProbe() {
  if (!canvasProbeActive_ || !canvas_ || !canvasBuffer_) return;
  const uint32_t now=millis();
  if (now-canvasLastFrameMs_<25) return;
  if (canvasFramesIssued_>canvasFramesRecorded_) {
    const uint32_t elapsed=now-canvasLastFrameMs_;
    canvasFrameTotalMs_+=elapsed; ++canvasFramesRecorded_;
    if (elapsed>canvasStats_.maximumFrameMs)
      canvasStats_.maximumFrameMs=elapsed>UINT16_MAX ? UINT16_MAX : elapsed;
    const uint32_t freeNow=ESP.getFreeHeap();
    if (freeNow<canvasStats_.minimumFree) canvasStats_.minimumFree=freeNow;
    if (canvasFramesRecorded_>=30) {
      canvasStats_.frameCount=canvasFramesRecorded_;
      canvasStats_.averageFrameMs=canvasFrameTotalMs_/canvasFramesRecorded_;
      canvasStats_.freeActive=ESP.getFreeHeap();
      canvasStats_.largestActive=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
      canvasProbeActive_=false; runtime_->replyCanvas(canvasStats_); return;
    }
  }
  if (canvasPreviousX_>=0)
    paintCanvasRect(canvasPreviousX_,canvasPreviousY_,24,24,canvasFramesIssued_,false);
  const int16_t x=(canvasFramesIssued_*17)%296;
  const int16_t y=(canvasFramesIssued_*11)%180;
  paintCanvasRect(x,y,24,24,canvasFramesIssued_,true);
  lv_draw_buf_flush_cache(canvasBuffer_,nullptr);
  lv_area_t coordinates; lv_obj_get_coords(canvas_,&coordinates);
  if (canvasPreviousX_>=0) {
    lv_area_t oldArea={coordinates.x1+canvasPreviousX_,coordinates.y1+canvasPreviousY_,
                       coordinates.x1+canvasPreviousX_+23,coordinates.y1+canvasPreviousY_+23};
    lv_obj_invalidate_area(canvas_,&oldArea);
  }
  lv_area_t newArea={coordinates.x1+x,coordinates.y1+y,
                     coordinates.x1+x+23,coordinates.y1+y+23};
  lv_obj_invalidate_area(canvas_,&newArea);
  canvasPreviousX_=x; canvasPreviousY_=y;
  ++canvasFramesIssued_; canvasLastFrameMs_=now;
}

void YapUiHost::rebuildWidgets() {
  if (!widgetRoot_ || !runtime_) return;
  bool structural=hostedWidgetCount_!=runtime_->widgetCount();
  for (uint8_t index=0;!structural && index<runtime_->widgetCount();++index) {
    const auto& item=runtime_->widgets()[index];
    structural=!widgetObjects_[index] || widgetKinds_[index]!=item.kind ||
      widgetRows_[index]!=item.rowCount ||
      reinterpret_cast<intptr_t>(lv_obj_get_user_data(widgetObjects_[index]))!=item.id;
  }
  if (!structural) {
    for (uint8_t index=0;index<runtime_->widgetCount();++index) {
      const auto& item=runtime_->widgets()[index]; auto* object=widgetObjects_[index];
      lv_obj_set_pos(object,item.x,item.y); lv_obj_set_size(object,item.width,item.height);
      if (item.kind==YapRuntimeService::UiKind::Label) {
        if (strcmp(lv_label_get_text(object),item.text)) lv_label_set_text(object,item.text);
      } else if (item.kind==YapRuntimeService::UiKind::Button) {
        auto* label=lv_obj_get_child(object,0);
        lv_obj_set_width(label,item.width-8);
        if (strcmp(lv_label_get_text(label),item.text)) lv_label_set_text(label,item.text);
      } else if (item.kind==YapRuntimeService::UiKind::Toggle) {
        auto* label=lv_obj_get_child(object,0); auto* toggle=lv_obj_get_child(object,1);
        if (strcmp(lv_label_get_text(label),item.text)) lv_label_set_text(label,item.text);
        lv_obj_align(label,LV_ALIGN_LEFT_MID,2,0); lv_obj_align(toggle,LV_ALIGN_RIGHT_MID,-2,0);
        if (item.checked) lv_obj_add_state(toggle,LV_STATE_CHECKED);
        else lv_obj_remove_state(toggle,LV_STATE_CHECKED);
      } else if (item.kind==YapRuntimeService::UiKind::TextField) {
        if (strcmp(lv_textarea_get_text(object),item.text)) lv_textarea_set_text(object,item.text);
      } else if (item.kind==YapRuntimeService::UiKind::List) {
        for (uint8_t row=0;row<item.rowCount;++row) {
          auto* entry=lv_obj_get_child(object,row); auto* label=lv_obj_get_child(entry,0);
          lv_obj_set_size(entry,item.width-4,28); lv_obj_set_width(label,item.width-12);
          if (strcmp(lv_label_get_text(label),item.rows[row])) lv_label_set_text(label,item.rows[row]);
          if (item.selected==row+1) lv_obj_add_state(entry,LV_STATE_CHECKED);
          else lv_obj_remove_state(entry,LV_STATE_CHECKED);
        }
      }
    }
    return;
  }
  lv_obj_clean(widgetRoot_); for (auto& object:widgetObjects_) object=nullptr;
  for (uint8_t index=0;index<runtime_->widgetCount();++index) {
    const auto& item=runtime_->widgets()[index]; lv_obj_t* object=nullptr;
    if (item.kind==YapRuntimeService::UiKind::Label) {
      object=lv_label_create(widgetRoot_); lv_label_set_text(object,item.text);
      lv_label_set_long_mode(object,LV_LABEL_LONG_WRAP);
      if (item.style==1) lv_obj_set_style_text_color(object,lv_color_hex(0x005A9E),0);
      if (item.style==2) {
        lv_obj_set_style_bg_opa(object,LV_OPA_COVER,0);
        lv_obj_set_style_bg_color(object,lv_color_hex(0xE7F1FA),0);
        lv_obj_set_style_pad_all(object,4,0); lv_obj_set_style_radius(object,3,0);
      }
    } else if (item.kind==YapRuntimeService::UiKind::Button) {
      object=lv_button_create(widgetRoot_); auto* label=lv_label_create(object);
      lv_label_set_text(label,item.text); lv_obj_set_width(label,item.width-8);
      lv_label_set_long_mode(label,LV_LABEL_LONG_DOT); lv_obj_center(label);
      lv_obj_add_event_cb(object,richEvent,LV_EVENT_SHORT_CLICKED,this);
      lv_obj_add_event_cb(object,richEvent,LV_EVENT_LONG_PRESSED,this);
    } else if (item.kind==YapRuntimeService::UiKind::Toggle) {
      object=lv_obj_create(widgetRoot_); lv_obj_set_style_pad_all(object,2,0);
      auto* label=lv_label_create(object); lv_label_set_text(label,item.text);
      lv_obj_align(label,LV_ALIGN_LEFT_MID,2,0);
      auto* toggle=lv_switch_create(object); lv_obj_align(toggle,LV_ALIGN_RIGHT_MID,-2,0);
      lv_obj_set_user_data(toggle,reinterpret_cast<void*>(static_cast<intptr_t>(item.id)));
      if (item.checked) lv_obj_add_state(toggle,LV_STATE_CHECKED);
      lv_obj_add_event_cb(toggle,richEvent,LV_EVENT_VALUE_CHANGED,this);
    } else if (item.kind==YapRuntimeService::UiKind::TextField) {
      object=lv_textarea_create(widgetRoot_); lv_textarea_set_text(object,item.text);
      lv_textarea_set_one_line(object,true); lv_textarea_set_max_length(object,96);
      lv_obj_add_event_cb(object,richEvent,LV_EVENT_CLICKED,this);
    } else if (item.kind==YapRuntimeService::UiKind::List) {
      object=lv_obj_create(widgetRoot_); lv_obj_set_style_pad_all(object,0,0);
      lv_obj_set_scroll_dir(object,LV_DIR_VER);
      for (uint8_t row=0;row<item.rowCount;++row) {
        auto* entry=lv_button_create(object); lv_obj_set_pos(entry,0,row*30);
        lv_obj_set_size(entry,item.width-4,28);
        const intptr_t tag=item.id | (static_cast<intptr_t>(row+1)<<8);
        lv_obj_set_user_data(entry,reinterpret_cast<void*>(tag));
        if (item.selected==row+1) lv_obj_add_state(entry,LV_STATE_CHECKED);
        lv_obj_add_event_cb(entry,richEvent,LV_EVENT_SHORT_CLICKED,this);
        auto* label=lv_label_create(entry); lv_label_set_text(label,item.rows[row]);
        lv_obj_set_width(label,item.width-12); lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);
        lv_obj_center(label);
      }
    }
    if (!object) continue;
    widgetObjects_[index]=object; lv_obj_set_pos(object,item.x,item.y);
    widgetKinds_[index]=item.kind; widgetRows_[index]=item.rowCount;
    lv_obj_set_size(object,item.width,item.height);
    lv_obj_set_style_text_font(object,font_,0);
    lv_obj_set_user_data(object,reinterpret_cast<void*>(static_cast<intptr_t>(item.id)));
    lv_obj_add_event_cb(object,richEvent,LV_EVENT_GESTURE,this);
  }
  hostedWidgetCount_=runtime_->widgetCount();
}

void YapUiHost::openTextField(uint8_t id) {
  const auto* item=widgetById(id); if (!item) return;
  newModal(tr("Edit text","Изменить текст")); fieldId_=id;
  textarea_=lv_textarea_create(modal_); lv_obj_set_pos(textarea_,6,24); lv_obj_set_size(textarea_,308,55);
  lv_obj_add_event_cb(textarea_,textEvent,LV_EVENT_READY,this);
  lv_textarea_set_max_length(textarea_,96); lv_textarea_set_text(textarea_,item->text);
  button(modal_,"OK",8,86,144,11); button(modal_,tr("Cancel","Отмена"),168,86,144,10);
  input_.setTarget(textarea_);
  if (!keyboard_->show(input_,russian_ ? KeyboardLanguage::Russian : KeyboardLanguage::English)) {
    dismiss(); fieldId_=0;
  }
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
  cancelCanvasIo();
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
  if (runtime_->request()!=YapRuntimeService::Request::CanvasProbe)
    canvasRequestStarted_=false;
  if (canvasIo_.kind!=CanvasIoKind::None) { updateCanvasIo(); return; }
  updateCanvasProbe();
  if (version_!=runtime_->uiVersion()) {
    version_=runtime_->uiVersion();
    if (richUi_) rebuildWidgets();
    else for (int i=0;i<6;++i) {
        const char* text=runtime_->buttons()[i].text;
        lv_label_set_text(lv_obj_get_child(buttons_[i],0),text);
        if (*text) lv_obj_remove_flag(buttons_[i],LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(buttons_[i],LV_OBJ_FLAG_HIDDEN);
      }
  }
  if (action==10) { runtime_->reply(nullptr,0,"cancelled"); dismiss(); return; }
  if (action==11 && textarea_) {
    const char* text=lv_textarea_get_text(textarea_);
    if (fieldId_) {
      if (strlen(text)>96) { error("text_too_long"); return; }
      const uint8_t id=fieldId_;
      if (!runtime_->setWidgetText(id,text)) { error("invalid_widget"); return; }
      runtime_->postUiEvent(id,YapRuntimeService::UiEventKind::Change,0,text);
      dismiss(); return;
    }
    if (strlen(text)>192) { error("text_too_long"); return; }
    runtime_->reply(text); dismiss(); return;
  }
  if (action==12 && textarea_) {
    keyboard_->show(input_,russian_ ? KeyboardLanguage::Russian : KeyboardLanguage::English);
    return;
  }
  if (action==13 || action==14) {
    runtime_->reply(nullptr,action==13 ? 1 : 0); dismiss(); return;
  }
  if (action==50 && fieldId_) { const uint8_t id=fieldId_; openTextField(id); return; }
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
  if (request==YapRuntimeService::Request::CanvasProbe) {
    if (!canvasRequestStarted_) beginCanvasProbe(runtime_->requestText());
    return;
  }
  if (request==YapRuntimeService::Request::CanvasRelease) {
    YapRuntimeService::CanvasStats stats={}; strlcpy(stats.format,"released",sizeof(stats.format));
    stats.freeBefore=ESP.getFreeHeap();
    stats.largestBefore=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    releaseCanvas();
    stats.freeActive=ESP.getFreeHeap();
    stats.largestActive=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    stats.minimumFree=stats.freeActive; runtime_->replyCanvas(stats); return;
  }
  if (request==YapRuntimeService::Request::CanvasCreate) {
    const auto& command=runtime_->canvasCommand();
    runtime_->replyCanvasCommand(createDrawingCanvas(command.width,command.height)
                                     ? nullptr : "out_of_memory");
    return;
  }
  if (request==YapRuntimeService::Request::CanvasClear) {
    if (!canvasBuffer_ || canvasFormat_!=LV_COLOR_FORMAT_I4)
      runtime_->replyCanvasCommand("no_canvas");
    else { clearDrawingCanvas(runtime_->canvasCommand().color); runtime_->replyCanvasCommand(); }
    return;
  }
  if (request==YapRuntimeService::Request::CanvasLine) {
    const auto& command=runtime_->canvasCommand();
    if (!canvasBuffer_ || canvasFormat_!=LV_COLOR_FORMAT_I4)
      runtime_->replyCanvasCommand("no_canvas");
    else if (command.x1>=canvasWidth_ || command.x2>=canvasWidth_ ||
             command.y1>=canvasHeight_ || command.y2>=canvasHeight_)
      runtime_->replyCanvasCommand("outside_canvas");
    else { drawCanvasLine(command); runtime_->replyCanvasCommand(); }
    return;
  }
  if (request==YapRuntimeService::Request::CanvasLoadBmp ||
      request==YapRuntimeService::Request::CanvasSaveBmp) {
    const char* errorText=nullptr;
    const int handle=runtime_->canvasCommand().handle;
    const bool started=request==YapRuntimeService::Request::CanvasLoadBmp
        ? beginCanvasBmpLoad(handle,errorText) : beginCanvasBmpSave(handle,errorText);
    if (!started) runtime_->replyCanvasCommand(errorText);
    return;
  }
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
  } else if (request==YapRuntimeService::Request::Confirm) {
    newModal(runtime_->requestText()); shown_=request;
    button(modal_,tr("Yes","Да"),8,82,144,13);
    button(modal_,tr("No","Нет"),168,82,144,14);
  } else { page_=0; strlcpy(directory_,"/Documents",sizeof(directory_)); picker(); }
}
