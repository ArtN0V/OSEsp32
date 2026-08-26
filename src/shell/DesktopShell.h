#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "../kernel/SystemKernel.h"
#include "../services/BootModeService.h"
#include "../services/SystemSettingsService.h"
#include "../services/StorageService.h"
#include "../services/TouchCalibrationService.h"
#include "../ui/LvglPort.h"

enum class ShellAppId : uint8_t {
  Files,
  Settings,
  SystemInfo,
  TextInput,
  About,
};

class DesktopShell {
 public:
  bool begin(SystemKernel& kernel, BootModeService& bootMode);
  void update();

 private:
  static DesktopShell* active_;

  SystemKernel* kernel_ = nullptr;
  BootModeService* bootMode_ = nullptr;
  SystemSettingsService settings_;
  StorageService storage_;
  LvglPort port_;
  TouchCalibrationService calibration_;

  lv_obj_t* screen_ = nullptr;
  lv_obj_t* taskLabel_ = nullptr;
  lv_obj_t* clockLabel_ = nullptr;
  lv_obj_t* startMenu_ = nullptr;
  lv_obj_t* wallpaper_ = nullptr;
  lv_obj_t* window_ = nullptr;
  lv_obj_t* dialog_ = nullptr;
  lv_obj_t* calibrationOverlay_ = nullptr;
  lv_obj_t* calibrationTitle_ = nullptr;
  lv_obj_t* calibrationTarget_ = nullptr;
  lv_obj_t* calibrationProgress_ = nullptr;
  bool calibrationPreviousPressed_ = false;
  bool rotation180_ = false;
  bool previousStorageMounted_ = false;
  StorageEntry fileEntries_[StorageService::PAGE_ENTRIES];
  uint8_t fileEntryCount_ = 0;
  uint16_t fileTotalCount_ = 0;
  uint8_t filePage_ = 0;
  char currentPath_[129] = "/";
  char selectedImagePath_[129] = {};
  char selectedImageLvPath_[132] = {};
  char wallpaperPath_[129] = {};
  char wallpaperLvPath_[132] = {};
  uint32_t lastClockSecond_ = UINT32_MAX;

  void buildDesktop();
  void buildTaskbar();
  void buildStartMenu();
  lv_obj_t* createButton(lv_obj_t* parent, const char* text, int16_t x,
                         int16_t y, int16_t width, int16_t height,
                         lv_event_cb_t callback, void* userData = nullptr);
  lv_obj_t* createWindow(const char* title);
  void closeWindow();
  void closeDialog();
  void showDiagnosticsDialog();
  void showInfoDialog(const char* message);
  void openApp(ShellAppId app);
  void openFiles();
  void openImage(const char* path);
  void openSettings();
  void openSystemInfo();
  void openTextInput();
  void openAbout();
  void updateClock();
  void setTaskText(const char* text);
  void applyWallpaper();
  void removeWallpaper();
  void parentDirectory();

  void startCalibration(bool ignoreCurrentPress);
  void updateCalibration();
  void drawCalibrationTarget();
  void finishCalibration(bool success);
  void cancelCalibration();

  static void startButtonEvent(lv_event_t* event);
  static void appButtonEvent(lv_event_t* event);
  static void closeButtonEvent(lv_event_t* event);
  static void brightnessEvent(lv_event_t* event);
  static void brightnessSaveEvent(lv_event_t* event);
  static void rotationEvent(lv_event_t* event);
  static void calibrateEvent(lv_event_t* event);
  static void resetCalibrationEvent(lv_event_t* event);
  static void diagnosticsEvent(lv_event_t* event);
  static void confirmDiagnosticsEvent(lv_event_t* event);
  static void cancelDialogEvent(lv_event_t* event);
  static void fileEntryEvent(lv_event_t* event);
  static void filesUpEvent(lv_event_t* event);
  static void filesPreviousEvent(lv_event_t* event);
  static void filesNextEvent(lv_event_t* event);
  static void imageBackEvent(lv_event_t* event);
  static void setWallpaperEvent(lv_event_t* event);
  static void clearWallpaperEvent(lv_event_t* event);
};
