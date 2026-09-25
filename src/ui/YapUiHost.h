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
  lv_obj_t* canvas_=nullptr;
  lv_draw_buf_t* canvasBuffer_=nullptr;
  lv_color_format_t canvasFormat_=LV_COLOR_FORMAT_UNKNOWN;
  uint16_t canvasWidth_=0, canvasHeight_=0;
  YapRuntimeService::CanvasStats canvasStats_;
  bool canvasProbeActive_=false, canvasRequestStarted_=false;
  uint8_t canvasFramesIssued_=0, canvasFramesRecorded_=0;
  int16_t canvasPreviousX_=-1, canvasPreviousY_=-1;
  int16_t canvasTouchX_=-1, canvasTouchY_=-1;
  uint32_t canvasLastFrameMs_=0, canvasFrameTotalMs_=0;
  enum class CanvasIoKind : uint8_t { None, LoadBmp, SaveBmp };
  struct CanvasIoState {
    CanvasIoKind kind=CanvasIoKind::None;
    int handle=0;
    uint32_t fileSize=0, pixelOffset=0, rowStride=0;
    uint32_t redMask=0, greenMask=0, blueMask=0;
    uint32_t sourceWidth=0, sourceHeight=0;
    uint16_t width=0, height=0, row=0, column=0, bitsPerPixel=0;
    uint16_t destinationX=0, destinationY=0;
    bool topDown=false;
  } canvasIo_;
  uint8_t canvasIoBuffer_[1280]={};
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
  void beginCanvasProbe(const char* format);
  bool createDrawingCanvas(uint16_t width,uint16_t height);
  void clearDrawingCanvas(uint8_t color);
  void drawCanvasLine(const YapRuntimeService::CanvasCommand& command);
  bool beginCanvasBmpLoad(int handle,const char*& error);
  bool beginCanvasBmpSave(int handle,const char*& error);
  void updateCanvasIo();
  void cancelCanvasIo();
  void updateCanvasProbe();
  void releaseCanvas();
  void setCanvasPixel(int16_t x,int16_t y,uint16_t frame,bool overlay);
  void setCanvasIndexPixel(int16_t x,int16_t y,uint8_t color);
  uint8_t canvasIndexPixel(int16_t x,int16_t y) const;
  void paintCanvasRect(int16_t x,int16_t y,int16_t width,int16_t height,
                       uint16_t frame,bool overlay);
  static void event(lv_event_t* event);
  static void richEvent(lv_event_t* event);
  static void textEvent(lv_event_t* event);
  static void canvasEvent(lv_event_t* event);
};
