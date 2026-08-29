#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "../kernel/SystemKernel.h"
#include "../services/BootModeService.h"
#include "../services/DateTimeService.h"
#include "../services/LocalizationService.h"
#include "../services/NotesService.h"
#include "../services/SystemSettingsService.h"
#include "../services/StorageService.h"
#include "../services/TouchCalibrationService.h"
#include "../services/WallpaperService.h"
#include "../services/YapPackageService.h"
#include "../runtime/YapRuntimeService.h"
#include "../ui/LvglPort.h"
#include "../ui/OSEsp32Font.h"
#include "../ui/SystemKeyboard.h"

enum class ShellAppId : uint8_t {
  Files,
  Settings,
  SystemInfo,
  Notes,
  About,
};

enum class ScreenSaverMode : uint8_t {
  Clock = 0,
  Picture = 1,
  Starfield = 2,
};

struct ScreenSaverStar {
  int16_t x = 0;
  int16_t y = 0;
  uint16_t z = 1;
  uint16_t previousZ = 1;
};

class DesktopShell {
 public:
  bool begin(SystemKernel& kernel, BootModeService& bootMode);
  void update();
  void setFullscreenApplicationActive(bool active);

 private:
  static DesktopShell* active_;

  SystemKernel* kernel_ = nullptr;
  BootModeService* bootMode_ = nullptr;
  SystemSettingsService settings_;
  LocalizationService localization_;
  DateTimeService dateTime_;
  StorageService storage_;
  NotesService notes_;
  WallpaperService wallpaperService_;
  YapPackageService yapPackages_;
  YapRuntimeService yapRuntime_;
  LvglPort port_;
  TouchCalibrationService calibration_;
  SystemKeyboard systemKeyboard_;
  LvglTextareaInputClient keyboardTestClient_;
  LvglTextareaInputClient noteKeyboardClient_;

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
  lv_obj_t* noteTitleArea_ = nullptr;
  lv_obj_t* noteBodyArea_ = nullptr;
  lv_obj_t* noteHideKeyboardButton_ = nullptr;
  lv_obj_t* dateTimeContent_ = nullptr;
  lv_obj_t* keyboardTestArea_ = nullptr;
  lv_obj_t* keyboardTestMetricsLabel_ = nullptr;
  lv_obj_t* screenSaverOverlay_ = nullptr;
  lv_obj_t* screenSaverTimeLabel_ = nullptr;
  lv_obj_t* screenSaverDateLabel_ = nullptr;
  lv_obj_t* screenSaverStarField_ = nullptr;
  bool calibrationPreviousPressed_ = false;
  bool rotation180_ = false;
  SystemLanguage language_ = SystemLanguage::English;
  uint8_t desktopColor_ = 0;
  bool noteEditorOpen_ = false;
  bool noteDirty_ = false;
  bool noteKeyboardVisible_ = false;
  bool screenSaverEnabled_ = false;
  bool screenSaverVisible_ = false;
  bool fullscreenApplicationActive_ = false;
  uint8_t screenSaverTimeoutIndex_ = 2;
  ScreenSaverMode screenSaverMode_ = ScreenSaverMode::Clock;
  static constexpr uint8_t SCREEN_SAVER_STAR_COUNT = 48;
  ScreenSaverStar* screenSaverStars_ = nullptr;
  uint32_t lastScreenSaverFrame_ = 0;
  bool previousStorageMounted_ = false;
  StorageEntry fileEntries_[StorageService::PAGE_ENTRIES];
  uint8_t fileEntryCount_ = 0;
  uint16_t fileTotalCount_ = 0;
  uint8_t filePage_ = 0;
  char currentPath_[129] = "/";
  char selectedImagePath_[129] = {};
  char selectedImageLvPath_[132] = {};
  char selectedYapPath_[129] = {};
  char wallpaperPath_[129] = {};
  char wallpaperLvPath_[132] = {};
  NoteSummary noteSummaries_[NotesService::MAX_NOTES];
  uint8_t noteCount_ = 0;
  char notePath_[129] = {};
  char noteTitle_[NotesService::TITLE_CAPACITY] = {};
  char noteBody_[NotesService::BODY_CAPACITY] = {};
  char pendingNoteDeletePath_[129] = {};
  char pendingNoteDeleteTitle_[NotesService::TITLE_CAPACITY] = {};
  bool noteDeleteConfirmed_ = false;
  bool yapRunRequested_ = false;
  char screenSaverImagePath_[129] = {};
  char screenSaverImageLvPath_[132] = {};
  struct tm pendingDateTime_ = {};
  int16_t pendingTimezoneMinutes_ = 180;
  bool dateTimeEditPrepared_ = false;
  int32_t dateTimeScrollY_ = 0;
  uint32_t lastClockSecond_ = UINT32_MAX;
  KeyboardLanguage keyboardTestLanguage_ = KeyboardLanguage::English;
  KeyboardLanguage keyboardLanguagePreference_ = KeyboardLanguage::English;

  void buildDesktop();
  void buildTaskbar();
  void buildStartMenu();
  lv_obj_t* createButton(lv_obj_t* parent, const char* text, int16_t x,
                         int16_t y, int16_t width, int16_t height,
                         lv_event_cb_t callback, void* userData = nullptr);
  lv_obj_t* createDesktopShortcut(const char* icon, const char* text,
                                  uint32_t iconColor, int16_t x, int16_t y,
                                  ShellAppId app);
  lv_obj_t* createSettingsRow(lv_obj_t* parent, const char* icon,
                              const char* title, const char* summary,
                              int16_t y, lv_event_cb_t callback);
  lv_obj_t* createColorChoice(lv_obj_t* parent, uint8_t colorIndex,
                              int16_t x, int16_t y);
  lv_obj_t* createWindow(const char* title);
  void closeWindow();
  void closeDialog();
  void showDiagnosticsDialog();
  void showInfoDialog(const char* message);
  void openApp(ShellAppId app);
  void openFiles();
  void openImage(const char* path);
  void openYapPackage(const char* path);
  void processPendingYapRun();
  void showYapRuntimeResult(const YapPackageInfo& package,
                            const YapRuntimeResult& result);
  void openSettings();
  void openDisplaySettings();
  void openDesktopColorSettings();
  void openLanguageSettings();
  void openTouchSettings();
  void openDateTimeSettings();
  void openScreenSaverSettings();
  void openSystemInfo();
  void openKeyboardTest();
  void openNotes();
  void openNoteEditor(const char* path = nullptr);
  void openAbout();
  void updateClock();
  void setTaskText(const char* text);
  void applyWallpaper();
  void removeWallpaper();
  void applyDesktopColor();
  void parentDirectory();
  void prepareDateTimeEdit();
  void adjustPendingDateTime(uint8_t field, int8_t delta);
  void updateScreenSaver();
  void showScreenSaver();
  void hideScreenSaver();
  void updateScreenSaverClock();
  void initializeScreenSaverStars();
  void updateScreenSaverStars();
  void resetScreenSaverStar(ScreenSaverStar& star, bool randomDepth);
  void showNoteKeyboard(lv_obj_t* textarea);
  void hideNoteKeyboard();
  bool saveCurrentNote();
  void requestNoteExit();
  void showNoteExitDialog();
  void showNoteDeleteDialog(uint8_t index);
  void processPendingNoteDelete();
  bool showKeyboardTestKeyboard();
  void updateKeyboardTestMetrics();
  const char* tr(const char* english, const char* russian) const {
    return localization_.text(english, russian);
  }
  const lv_font_t* uiFont() const {
    return &osesp32_font_14;
  }
  const lv_font_t* uiSmallFont() const {
    return &osesp32_font_12;
  }

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
  static void desktopColorSettingsEvent(lv_event_t* event);
  static void desktopColorEvent(lv_event_t* event);
  static void settingsDisplayEvent(lv_event_t* event);
  static void settingsLanguageEvent(lv_event_t* event);
  static void settingsTouchEvent(lv_event_t* event);
  static void settingsDateTimeEvent(lv_event_t* event);
  static void settingsScreenSaverEvent(lv_event_t* event);
  static void settingsBackEvent(lv_event_t* event);
  static void languageEnglishEvent(lv_event_t* event);
  static void languageRussianEvent(lv_event_t* event);
  static void calibrateEvent(lv_event_t* event);
  static void resetCalibrationEvent(lv_event_t* event);
  static void diagnosticsEvent(lv_event_t* event);
  static void keyboardTestEvent(lv_event_t* event);
  static void keyboardTestShowEvent(lv_event_t* event);
  static void keyboardTestHideEvent(lv_event_t* event);
  static void keyboardTestLanguageEvent(lv_event_t* event);
  static void keyboardTestTextChangedEvent(lv_event_t* event);
  static void keyboardTestVisibilityChanged(bool visible,
                                            uint16_t coveredHeight,
                                            void* userData);
  static void confirmDiagnosticsEvent(lv_event_t* event);
  static void cancelDialogEvent(lv_event_t* event);
  static void fileEntryEvent(lv_event_t* event);
  static void yapRunEvent(lv_event_t* event);
  static void filesUpEvent(lv_event_t* event);
  static void filesPreviousEvent(lv_event_t* event);
  static void filesNextEvent(lv_event_t* event);
  static void imageBackEvent(lv_event_t* event);
  static void setWallpaperEvent(lv_event_t* event);
  static void clearWallpaperEvent(lv_event_t* event);
  static void setScreenSaverImageEvent(lv_event_t* event);
  static void dateTimeAdjustEvent(lv_event_t* event);
  static void dateTimeSaveEvent(lv_event_t* event);
  static void screenSaverEnabledEvent(lv_event_t* event);
  static void screenSaverTimeoutEvent(lv_event_t* event);
  static void screenSaverModeEvent(lv_event_t* event);
  static void screenSaverChooseImageEvent(lv_event_t* event);
  static void screenSaverClearImageEvent(lv_event_t* event);
  static void screenSaverWakeEvent(lv_event_t* event);
  static void screenSaverDrawStarsEvent(lv_event_t* event);
  static void noteCardEvent(lv_event_t* event);
  static void noteDeleteRequestEvent(lv_event_t* event);
  static void noteDeleteConfirmEvent(lv_event_t* event);
  static void noteTextChangedEvent(lv_event_t* event);
  static void noteTextFocusedEvent(lv_event_t* event);
  static void noteTitleReadyEvent(lv_event_t* event);
  static void noteKeyboardVisibilityChanged(bool visible,
                                             uint16_t coveredHeight,
                                             void* userData);
  static void noteBackEvent(lv_event_t* event);
  static void noteSaveEvent(lv_event_t* event);
  static void noteHideKeyboardEvent(lv_event_t* event);
  static void noteExitSaveEvent(lv_event_t* event);
  static void noteExitDiscardEvent(lv_event_t* event);
};
