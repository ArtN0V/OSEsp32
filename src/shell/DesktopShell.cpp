#include "DesktopShell.h"

#include <esp_heap_caps.h>
#include <esp_system.h>

#include "../board/BoardConfig.h"

namespace {
constexpr uint32_t COLOR_DESKTOP_TOP = 0x0B4F83;
constexpr uint32_t COLOR_DESKTOP_BOTTOM = 0x007C91;
constexpr uint32_t COLOR_TASKBAR = 0x17212B;
constexpr uint32_t COLOR_WINDOW = 0xF3F4F6;
constexpr uint32_t COLOR_TITLE = 0x0067B8;
constexpr uint32_t COLOR_ACCENT = 0x0078D4;
constexpr uint32_t COLOR_DANGER = 0xC42B1C;
constexpr int16_t TASKBAR_Y = 204;
constexpr int16_t TASKBAR_HEIGHT = 36;
constexpr int16_t WINDOW_X = 5;
constexpr int16_t WINDOW_Y = 5;
constexpr int16_t WINDOW_WIDTH = 310;
constexpr int16_t WINDOW_HEIGHT = 196;

void configurePanel(lv_obj_t* object, uint32_t color, int radius = 0) {
  lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(object, radius, 0);
  lv_obj_set_style_border_width(object, 0, 0);
  lv_obj_set_style_pad_all(object, 0, 0);
  lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}
}  // namespace

DesktopShell* DesktopShell::active_ = nullptr;

bool DesktopShell::begin(SystemKernel& kernel, BootModeService& bootMode) {
  active_ = this;
  kernel_ = &kernel;
  bootMode_ = &bootMode;
  rotation180_ = settings_.loadRotation180();
  if (!port_.begin(rotation180_)) {
    kernel_->faults().report(FaultCode::InternalError, "shell",
                             "LVGL port initialization failed");
    return false;
  }

  storage_.begin(kernel_->events(), kernel_->logger());
  storage_.registerLvglDriver();
  previousStorageMounted_ = storage_.mounted();
  calibration_.begin(port_.touchDriver(), kernel_->events(), kernel_->logger(),
                     rotation180_);
  port_.displayDriver().setBrightness(settings_.loadBrightness());
  buildDesktop();
  kernel_->setLifecycle(LifecycleState::Running);
  kernel_->logger().info("shell", "desktop created; LVGL %u.%u.%u",
                         lv_version_major(), lv_version_minor(),
                         lv_version_patch());
  kernel_->events().publish(SystemEventType::ShellReady, ESP.getFreeHeap());

  if (!port_.touchDriver().calibration().stored) startCalibration(false);
  return true;
}

void DesktopShell::update() {
  port_.update();
  storage_.update();
  if (storage_.mounted() != previousStorageMounted_) {
    previousStorageMounted_ = storage_.mounted();
    if (previousStorageMounted_)
      applyWallpaper();
    else {
      closeWindow();
      removeWallpaper();
      setTaskText("SD card removed");
    }
  }
  if (calibration_.active()) updateCalibration();
  updateClock();
  delay(2);
}

lv_obj_t* DesktopShell::createButton(lv_obj_t* parent, const char* text,
                                     int16_t x, int16_t y, int16_t width,
                                     int16_t height, lv_event_cb_t callback,
                                     void* userData) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, width, height);
  lv_obj_set_style_radius(button, 3, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(COLOR_ACCENT), 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x005A9E), LV_STATE_PRESSED);
  lv_obj_set_style_pad_all(button, 2, 0);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, userData);

  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(label);
  return button;
}

void DesktopShell::buildDesktop() {
  screen_ = lv_screen_active();
  configurePanel(screen_, COLOR_DESKTOP_TOP);
  lv_obj_set_style_bg_grad_color(screen_, lv_color_hex(COLOR_DESKTOP_BOTTOM), 0);
  lv_obj_set_style_bg_grad_dir(screen_, LV_GRAD_DIR_VER, 0);
  applyWallpaper();

  lv_obj_t* brand = lv_label_create(screen_);
  lv_label_set_text(brand, "OSEsp32");
  lv_obj_set_pos(brand, 218, 8);
  lv_obj_set_style_text_color(brand, lv_color_hex(0xBFE9FF), 0);

  createButton(screen_, "FILES", 12, 30, 72, 48, appButtonEvent,
               reinterpret_cast<void*>(static_cast<uintptr_t>(ShellAppId::Files)));
  createButton(screen_, "SETTINGS", 94, 30, 84, 48, appButtonEvent,
               reinterpret_cast<void*>(static_cast<uintptr_t>(ShellAppId::Settings)));
  createButton(screen_, "SYSTEM\nINFO", 12, 88, 72, 48, appButtonEvent,
               reinterpret_cast<void*>(static_cast<uintptr_t>(ShellAppId::SystemInfo)));
  createButton(screen_, "TEXT\nINPUT", 94, 88, 84, 48, appButtonEvent,
               reinterpret_cast<void*>(static_cast<uintptr_t>(ShellAppId::TextInput)));

  lv_obj_t* hint = lv_label_create(screen_);
  lv_label_set_text(hint, "Touch Start to open applications");
  lv_obj_set_pos(hint, 12, 176);
  lv_obj_set_style_text_color(hint, lv_color_hex(0xD9F2FF), 0);

  buildTaskbar();
  buildStartMenu();
  updateClock();
}

void DesktopShell::buildTaskbar() {
  lv_obj_t* taskbar = lv_obj_create(screen_);
  lv_obj_set_pos(taskbar, 0, TASKBAR_Y);
  lv_obj_set_size(taskbar, board::SCREEN_WIDTH, TASKBAR_HEIGHT);
  configurePanel(taskbar, COLOR_TASKBAR);

  createButton(taskbar, "START", 3, 3, 66, 30, startButtonEvent);
  taskLabel_ = lv_label_create(taskbar);
  lv_label_set_text(taskLabel_, "Desktop");
  lv_obj_set_pos(taskLabel_, 76, 10);
  lv_obj_set_width(taskLabel_, 170);
  lv_label_set_long_mode(taskLabel_, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(taskLabel_, lv_color_white(), 0);

  clockLabel_ = lv_label_create(taskbar);
  lv_obj_set_pos(clockLabel_, 260, 10);
  lv_obj_set_width(clockLabel_, 56);
  lv_obj_set_style_text_align(clockLabel_, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_color(clockLabel_, lv_color_white(), 0);
}

void DesktopShell::buildStartMenu() {
  startMenu_ = lv_obj_create(screen_);
  lv_obj_set_pos(startMenu_, 3, 31);
  lv_obj_set_size(startMenu_, 174, 173);
  configurePanel(startMenu_, 0x202B36, 4);
  lv_obj_set_style_border_width(startMenu_, 1, 0);
  lv_obj_set_style_border_color(startMenu_, lv_color_hex(0x607080), 0);

  lv_obj_t* title = lv_label_create(startMenu_);
  lv_label_set_text(title, "OSEsp32 applications");
  lv_obj_set_pos(title, 8, 7);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);

  const char* names[] = {"Files", "Settings", "System Info", "Text Input",
                         "About"};
  for (uint8_t index = 0; index < 5; ++index) {
    createButton(startMenu_, names[index], 7, 28 + index * 28, 160, 25,
                 appButtonEvent,
                 reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
  }
  lv_obj_add_flag(startMenu_, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t* DesktopShell::createWindow(const char* title) {
  closeWindow();
  if (!lv_obj_has_flag(startMenu_, LV_OBJ_FLAG_HIDDEN))
    lv_obj_add_flag(startMenu_, LV_OBJ_FLAG_HIDDEN);

  window_ = lv_obj_create(screen_);
  lv_obj_set_pos(window_, WINDOW_X, WINDOW_Y);
  lv_obj_set_size(window_, WINDOW_WIDTH, WINDOW_HEIGHT);
  configurePanel(window_, COLOR_WINDOW, 3);
  lv_obj_set_style_border_width(window_, 1, 0);
  lv_obj_set_style_border_color(window_, lv_color_hex(0x404040), 0);

  lv_obj_t* titlebar = lv_obj_create(window_);
  lv_obj_set_pos(titlebar, 0, 0);
  lv_obj_set_size(titlebar, WINDOW_WIDTH - 2, 31);
  configurePanel(titlebar, COLOR_TITLE, 2);
  lv_obj_t* titleLabel = lv_label_create(titlebar);
  lv_label_set_text(titleLabel, title);
  lv_obj_set_pos(titleLabel, 8, 8);
  lv_obj_set_style_text_color(titleLabel, lv_color_white(), 0);
  lv_obj_t* close = createButton(titlebar, "X", 270, 2, 34, 27,
                                 closeButtonEvent);
  lv_obj_set_style_bg_color(close, lv_color_hex(COLOR_DANGER), 0);

  lv_obj_t* content = lv_obj_create(window_);
  lv_obj_set_pos(content, 1, 32);
  lv_obj_set_size(content, WINDOW_WIDTH - 4, WINDOW_HEIGHT - 35);
  configurePanel(content, COLOR_WINDOW);
  lv_obj_set_style_text_color(content, lv_color_hex(0x202020), 0);
  setTaskText(title);
  return content;
}

void DesktopShell::closeWindow() {
  closeDialog();
  if (window_) {
    lv_obj_delete(window_);
    window_ = nullptr;
  }
  setTaskText("Desktop");
}

void DesktopShell::closeDialog() {
  if (dialog_) {
    lv_obj_delete(dialog_);
    dialog_ = nullptr;
  }
}

void DesktopShell::showDiagnosticsDialog() {
  closeDialog();
  dialog_ = lv_obj_create(screen_);
  lv_obj_set_pos(dialog_, 35, 57);
  lv_obj_set_size(dialog_, 250, 118);
  configurePanel(dialog_, 0xF7F7F7, 4);
  lv_obj_set_style_border_width(dialog_, 2, 0);
  lv_obj_set_style_border_color(dialog_, lv_color_hex(COLOR_TITLE), 0);

  lv_obj_t* message = lv_label_create(dialog_);
  lv_label_set_text(message,
                    "Restart into hardware diagnostics?\n"
                    "The next boot returns to the shell.");
  lv_obj_set_pos(message, 12, 12);
  lv_obj_set_style_text_color(message, lv_color_hex(0x202020), 0);
  createButton(dialog_, "CANCEL", 18, 75, 92, 32, cancelDialogEvent);
  createButton(dialog_, "RESTART", 139, 75, 92, 32,
               confirmDiagnosticsEvent);
  lv_obj_move_foreground(dialog_);
}

void DesktopShell::showInfoDialog(const char* text) {
  closeDialog();
  dialog_ = lv_obj_create(screen_);
  lv_obj_set_pos(dialog_, 35, 62);
  lv_obj_set_size(dialog_, 250, 108);
  configurePanel(dialog_, 0xF7F7F7, 4);
  lv_obj_set_style_border_width(dialog_, 2, 0);
  lv_obj_set_style_border_color(dialog_, lv_color_hex(COLOR_TITLE), 0);
  lv_obj_t* message = lv_label_create(dialog_);
  lv_label_set_text(message, text);
  lv_obj_set_pos(message, 12, 12);
  lv_obj_set_width(message, 226);
  lv_obj_set_style_text_color(message, lv_color_hex(0x202020), 0);
  createButton(dialog_, "OK", 79, 68, 92, 30, cancelDialogEvent);
  lv_obj_move_foreground(dialog_);
}

void DesktopShell::openApp(ShellAppId app) {
  kernel_->events().publish(SystemEventType::ShellApplicationOpened,
                            static_cast<uint32_t>(app));
  switch (app) {
    case ShellAppId::Files: openFiles(); break;
    case ShellAppId::Settings: openSettings(); break;
    case ShellAppId::SystemInfo: openSystemInfo(); break;
    case ShellAppId::TextInput: openTextInput(); break;
    case ShellAppId::About: openAbout(); break;
  }
}

void DesktopShell::openFiles() {
  lv_obj_t* content = createWindow("Files");
  if (!storage_.mounted()) {
    lv_obj_t* label = lv_label_create(content);
    lv_label_set_text(label, "SD card is not available.\nInsert a FAT32 card and reopen Files.");
    lv_obj_set_pos(label, 14, 24);
    return;
  }

  constexpr uint8_t entriesPerPage = StorageService::PAGE_ENTRIES;
  if (!storage_.listDirectoryPage(currentPath_, filePage_ * entriesPerPage,
                                  fileEntries_, entriesPerPage,
                                  fileEntryCount_, fileTotalCount_)) {
    lv_obj_t* label = lv_label_create(content);
    lv_label_set_text(label, "Unable to read this directory.");
    lv_obj_set_pos(label, 14, 24);
    return;
  }

  const uint8_t pageCount =
      fileTotalCount_ == 0 ? 1 : (fileTotalCount_ + entriesPerPage - 1) / entriesPerPage;
  if (filePage_ >= pageCount) {
    filePage_ = pageCount - 1;
    if (!storage_.listDirectoryPage(currentPath_, filePage_ * entriesPerPage,
                                    fileEntries_, entriesPerPage,
                                    fileEntryCount_, fileTotalCount_)) {
      return;
    }
  }

  lv_obj_t* path = lv_label_create(content);
  lv_label_set_text(path, currentPath_);
  lv_obj_set_pos(path, 7, 4);
  lv_obj_set_width(path, 290);
  lv_label_set_long_mode(path, LV_LABEL_LONG_DOT);

  for (uint8_t row = 0; row < entriesPerPage; ++row) {
    const uint8_t index = row;
    if (index >= fileEntryCount_) break;
    char caption[64];
    snprintf(caption, sizeof(caption), "%s%s",
             fileEntries_[index].directory ? "[DIR] " : "", fileEntries_[index].name);
    createButton(content, caption, 5, 24 + row * 26, 296, 23,
                 fileEntryEvent, reinterpret_cast<void*>(static_cast<uintptr_t>(index)));
  }

  createButton(content, "UP", 5, 132, 58, 27, filesUpEvent);
  createButton(content, "<", 70, 132, 42, 27, filesPreviousEvent);
  char pages[20];
  snprintf(pages, sizeof(pages), "%u / %u", filePage_ + 1, pageCount);
  lv_obj_t* page = lv_label_create(content);
  lv_label_set_text(page, pages);
  lv_obj_set_pos(page, 130, 140);
  createButton(content, ">", 257, 132, 42, 27, filesNextEvent);
}

void DesktopShell::openImage(const char* path) {
  if (!path || !storage_.mounted()) return;
  strlcpy(selectedImagePath_, path, sizeof(selectedImagePath_));
  if (!StorageService::makeLvglPath(selectedImagePath_, selectedImageLvPath_,
                                    sizeof(selectedImageLvPath_))) {
    showInfoDialog("Image path is too long.");
    return;
  }
  lv_image_header_t header;
  if (lv_image_decoder_get_info(selectedImageLvPath_, &header) != LV_RESULT_OK) {
    showInfoDialog("Unsupported image.\nUse lowercase .bmp, .jpg or .jpeg.");
    return;
  }

  lv_obj_t* content = createWindow("Image Viewer");
  lv_obj_t* image = lv_image_create(content);
  lv_obj_set_pos(image, 3, 2);
  lv_obj_set_size(image, 300, 120);
  lv_obj_set_style_bg_color(image, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(image, LV_OPA_COVER, 0);
  lv_image_set_inner_align(image, LV_IMAGE_ALIGN_CENTER);
  lv_image_set_src(image, selectedImageLvPath_);
  createButton(content, "FILES", 5, 127, 70, 30, imageBackEvent);
  createButton(content, "SET WALLPAPER", 105, 127, 194, 30,
               setWallpaperEvent);
}

void DesktopShell::openSettings() {
  lv_obj_t* content = createWindow("Settings");
  lv_obj_t* heading = lv_label_create(content);
  lv_label_set_text(heading, "Backlight");
  lv_obj_set_pos(heading, 12, 8);

  lv_obj_t* slider = lv_slider_create(content);
  lv_obj_set_pos(slider, 92, 10);
  lv_obj_set_size(slider, 190, 16);
  lv_slider_set_range(slider, 25, 255);
  lv_slider_set_value(slider, port_.displayDriver().getBrightness(),
                      LV_ANIM_OFF);
  lv_obj_add_event_cb(slider, brightnessEvent, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(slider, brightnessSaveEvent, LV_EVENT_RELEASED, nullptr);

  createButton(content, "CALIBRATE TOUCH", 12, 45, 138, 34,
               calibrateEvent);
  createButton(content, "RESET + CALIBRATE", 158, 45, 135, 34,
               resetCalibrationEvent);

  createButton(content, rotation180_ ? "ROTATE TO 0" : "ROTATE TO 180",
               12, 85, 138, 30, rotationEvent);
  createButton(content, "CLEAR WALLPAPER", 158, 85, 135, 30,
               clearWallpaperEvent);

  const TouchCalibration& calibration = port_.touchDriver().calibration();
  char details[128];
  snprintf(details, sizeof(details),
           "Touch: %s\nX %u..%u%s   Y %u..%u%s",
           calibration.stored ? "saved in NVS" : "board defaults",
           calibration.rawXMin, calibration.rawXMax,
           calibration.invertX ? " inv" : "", calibration.rawYMin,
           calibration.rawYMax, calibration.invertY ? " inv" : "");
  lv_obj_t* detailLabel = lv_label_create(content);
  lv_label_set_text(detailLabel, details);
  lv_obj_set_pos(detailLabel, 12, 122);
}

void DesktopShell::openSystemInfo() {
  lv_obj_t* content = createWindow("System Info");
  const MemorySnapshot memory = kernel_->monitor().sample();
  char info[256];
  snprintf(info, sizeof(info),
           "ESP32  %u MHz   Flash %u MiB\n"
           "Heap free %u KiB   minimum %u KiB\n"
           "Largest block %u KiB\n"
           "Events %lu   dropped %lu   faults %lu",
           ESP.getCpuFreqMHz(), ESP.getFlashChipSize() / 1048576,
           memory.freeHeap / 1024, memory.minimumFreeHeap / 1024,
           memory.largestFreeBlock / 1024,
           static_cast<unsigned long>(kernel_->handledEventCount()),
           static_cast<unsigned long>(kernel_->events().droppedCount()),
           static_cast<unsigned long>(kernel_->faults().count()));
  lv_obj_t* label = lv_label_create(content);
  lv_label_set_text(label, info);
  lv_obj_set_pos(label, 10, 8);
  createButton(content, "HARDWARE DIAGNOSTICS", 42, 104, 220, 36,
               diagnosticsEvent);
}

void DesktopShell::openTextInput() {
  lv_obj_t* content = createWindow("Text Input");
  lv_obj_t* textarea = lv_textarea_create(content);
  lv_obj_set_pos(textarea, 5, 2);
  lv_obj_set_size(textarea, 295, 31);
  lv_textarea_set_one_line(textarea, true);
  lv_textarea_set_placeholder_text(textarea, "Touch here and type...");

  lv_obj_t* keyboard = lv_keyboard_create(content);
  lv_obj_set_pos(keyboard, 3, 35);
  lv_obj_set_size(keyboard, 299, 123);
  lv_obj_set_style_pad_all(keyboard, 2, 0);
  lv_obj_set_style_pad_row(keyboard, 2, 0);
  lv_obj_set_style_pad_column(keyboard, 2, 0);
  lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_12, LV_PART_ITEMS);
  lv_keyboard_set_textarea(keyboard, textarea);
  lv_obj_add_state(textarea, LV_STATE_FOCUSED);
}

void DesktopShell::openAbout() {
  lv_obj_t* content = createWindow("About OSEsp32");
  lv_obj_t* title = lv_label_create(content);
  lv_label_set_text(title, "OSEsp32 0.3\nStorage and files preview");
  lv_obj_set_pos(title, 18, 14);
  lv_obj_set_style_text_color(title, lv_color_hex(COLOR_TITLE), 0);

  lv_obj_t* text = lv_label_create(content);
  lv_label_set_text(text,
                    "Arduino-ESP32 + FreeRTOS + LVGL\n"
                    "ESP32-2432S028 / ILI9341 / XPT2046\n\n"
                    "External .yap applications: Stage 4");
  lv_obj_set_pos(text, 18, 58);
}

void DesktopShell::updateClock() {
  if (!clockLabel_) return;
  const uint32_t seconds = millis() / 1000;
  if (seconds == lastClockSecond_) return;
  lastClockSecond_ = seconds;
  char clock[12];
  snprintf(clock, sizeof(clock), "%02lu:%02lu",
           static_cast<unsigned long>((seconds / 60) % 100),
           static_cast<unsigned long>(seconds % 60));
  lv_label_set_text(clockLabel_, clock);
}

void DesktopShell::setTaskText(const char* text) {
  if (taskLabel_) lv_label_set_text(taskLabel_, text);
}

void DesktopShell::applyWallpaper() {
  removeWallpaper();
  if (!storage_.mounted() ||
      !settings_.loadWallpaper(wallpaperPath_, sizeof(wallpaperPath_)) ||
      !storage_.exists(wallpaperPath_) ||
      !StorageService::makeLvglPath(wallpaperPath_, wallpaperLvPath_,
                                    sizeof(wallpaperLvPath_))) {
    return;
  }

  lv_image_header_t header;
  if (lv_image_decoder_get_info(wallpaperLvPath_, &header) != LV_RESULT_OK)
    return;
  wallpaper_ = lv_image_create(screen_);
  lv_obj_set_pos(wallpaper_, 0, 0);
  lv_obj_set_size(wallpaper_, board::SCREEN_WIDTH, TASKBAR_Y);
  lv_image_set_inner_align(wallpaper_, LV_IMAGE_ALIGN_CENTER);
  lv_image_set_src(wallpaper_, wallpaperLvPath_);
  lv_obj_remove_flag(wallpaper_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_move_to_index(wallpaper_, 0);
}

void DesktopShell::removeWallpaper() {
  if (wallpaper_) {
    lv_obj_delete(wallpaper_);
    wallpaper_ = nullptr;
  }
}

void DesktopShell::parentDirectory() {
  if (!strcmp(currentPath_, "/")) return;
  char* slash = strrchr(currentPath_, '/');
  if (!slash || slash == currentPath_)
    strlcpy(currentPath_, "/", sizeof(currentPath_));
  else
    *slash = '\0';
  filePage_ = 0;
}

void DesktopShell::startCalibration(bool ignoreCurrentPress) {
  closeWindow();
  lv_obj_add_flag(startMenu_, LV_OBJ_FLAG_HIDDEN);
  port_.suspendPointer(true);
  calibrationPreviousPressed_ = port_.touchPressed();
  calibration_.start(ignoreCurrentPress);

  calibrationOverlay_ = lv_obj_create(screen_);
  lv_obj_set_pos(calibrationOverlay_, 0, 0);
  lv_obj_set_size(calibrationOverlay_, board::SCREEN_WIDTH,
                  board::SCREEN_HEIGHT);
  configurePanel(calibrationOverlay_, 0x101820);

  calibrationTitle_ = lv_label_create(calibrationOverlay_);
  lv_obj_set_pos(calibrationTitle_, 86, 88);
  lv_obj_set_width(calibrationTitle_, 148);
  lv_obj_set_style_text_align(calibrationTitle_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(calibrationTitle_, lv_color_white(), 0);

  lv_obj_t* instruction = lv_label_create(calibrationOverlay_);
  lv_label_set_text(instruction, "Hold center, then release stylus");
  lv_obj_set_pos(instruction, 45, 112);
  lv_obj_set_style_text_color(instruction, lv_color_hex(0xB8C7D1), 0);

  calibrationProgress_ = lv_bar_create(calibrationOverlay_);
  lv_obj_set_pos(calibrationProgress_, 100, 138);
  lv_obj_set_size(calibrationProgress_, 120, 7);
  lv_bar_set_range(calibrationProgress_, 0, 100);

  lv_obj_t* cancel = createButton(calibrationOverlay_, "CANCEL", 122, 3, 76,
                                  27, cancelDialogEvent);
  lv_obj_remove_flag(cancel, LV_OBJ_FLAG_CLICKABLE);

  calibrationTarget_ = lv_obj_create(calibrationOverlay_);
  lv_obj_set_size(calibrationTarget_, 30, 30);
  configurePanel(calibrationTarget_, 0x101820, LV_RADIUS_CIRCLE);
  lv_obj_set_style_border_width(calibrationTarget_, 2, 0);
  lv_obj_set_style_border_color(calibrationTarget_, lv_color_hex(0xFFD43B), 0);
  lv_obj_t* cross = lv_label_create(calibrationTarget_);
  lv_label_set_text(cross, "+");
  lv_obj_set_style_text_color(cross, lv_color_white(), 0);
  lv_obj_center(cross);
  drawCalibrationTarget();
  lv_obj_move_foreground(calibrationOverlay_);
}

void DesktopShell::drawCalibrationTarget() {
  if (!calibrationOverlay_) return;
  char title[32];
  snprintf(title, sizeof(title), "TOUCH CALIBRATION %u / %u",
           calibration_.pointIndex() + 1,
           TouchCalibrationService::POINT_COUNT);
  lv_label_set_text(calibrationTitle_, title);
  lv_obj_set_pos(calibrationTarget_, calibration_.targetX() - 15,
                 calibration_.targetY() - 15);
  lv_bar_set_value(calibrationProgress_, 0, LV_ANIM_OFF);
}

void DesktopShell::updateCalibration() {
  const bool pressed = port_.touchPressed();
  const bool newPress = pressed && !calibrationPreviousPressed_;
  calibrationPreviousPressed_ = pressed;
  const TouchPoint& point = port_.touchPoint();
  if (newPress && point.x >= 120 && point.x <= 200 && point.y < 34) {
    cancelCalibration();
    return;
  }
  const CalibrationUpdate result = calibration_.update(
      pressed, point);
  switch (result) {
    case CalibrationUpdate::Sampling: {
      const uint16_t samples = calibration_.sampleCount();
      const uint16_t progress = samples * 100 / 24;
      lv_bar_set_value(calibrationProgress_, progress > 100 ? 100 : progress,
                       LV_ANIM_OFF);
      break;
    }
    case CalibrationUpdate::PointAccepted:
    case CalibrationUpdate::PointRetry:
      drawCalibrationTarget();
      break;
    case CalibrationUpdate::Completed:
      finishCalibration(true);
      break;
    case CalibrationUpdate::Failed:
      finishCalibration(false);
      break;
    case CalibrationUpdate::None:
      break;
  }
}

void DesktopShell::finishCalibration(bool success) {
  if (calibrationOverlay_) lv_obj_delete(calibrationOverlay_);
  calibrationOverlay_ = nullptr;
  calibrationTitle_ = nullptr;
  calibrationTarget_ = nullptr;
  calibrationProgress_ = nullptr;
  port_.suspendPointer(false);
  setTaskText(success ? "Touch calibration saved" : "Calibration failed");
  kernel_->logger().info("shell", "touch calibration UI finished: %s",
                         success ? "saved" : "failed");
}

void DesktopShell::cancelCalibration() {
  calibration_.cancel();
  if (calibrationOverlay_) lv_obj_delete(calibrationOverlay_);
  calibrationOverlay_ = nullptr;
  calibrationTitle_ = nullptr;
  calibrationTarget_ = nullptr;
  calibrationProgress_ = nullptr;
  port_.suspendPointer(false);
  setTaskText("Touch calibration cancelled");
}

void DesktopShell::startButtonEvent(lv_event_t*) {
  if (lv_obj_has_flag(active_->startMenu_, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_remove_flag(active_->startMenu_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(active_->startMenu_);
  } else {
    lv_obj_add_flag(active_->startMenu_, LV_OBJ_FLAG_HIDDEN);
  }
}

void DesktopShell::appButtonEvent(lv_event_t* event) {
  const uintptr_t value = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
  active_->openApp(static_cast<ShellAppId>(value));
}

void DesktopShell::closeButtonEvent(lv_event_t*) { active_->closeWindow(); }

void DesktopShell::brightnessEvent(lv_event_t* event) {
  const int32_t value = lv_slider_get_value(lv_event_get_target_obj(event));
  active_->port_.displayDriver().setBrightness(value);
}

void DesktopShell::brightnessSaveEvent(lv_event_t* event) {
  const int32_t value = lv_slider_get_value(lv_event_get_target_obj(event));
  if (!active_->settings_.saveBrightness(static_cast<uint8_t>(value))) {
    active_->kernel_->faults().report(FaultCode::StorageUnavailable,
                                      "settings",
                                      "brightness could not be saved");
  }
}

void DesktopShell::rotationEvent(lv_event_t*) {
  if (!active_->settings_.saveRotation180(!active_->rotation180_)) {
    active_->kernel_->faults().report(FaultCode::StorageUnavailable,
                                      "settings",
                                      "rotation could not be saved");
    return;
  }
  active_->setTaskText("Applying rotation...");
  delay(150);
  ESP.restart();
}

void DesktopShell::calibrateEvent(lv_event_t*) {
  active_->startCalibration(true);
}

void DesktopShell::resetCalibrationEvent(lv_event_t*) {
  active_->port_.touchDriver().resetCalibration();
  active_->startCalibration(true);
}

void DesktopShell::diagnosticsEvent(lv_event_t*) {
  active_->showDiagnosticsDialog();
}

void DesktopShell::confirmDiagnosticsEvent(lv_event_t*) {
  if (!active_->bootMode_->requestDiagnostics()) {
    active_->setTaskText("Could not request diagnostics");
    active_->kernel_->faults().report(FaultCode::StorageUnavailable, "boot",
                                      "diagnostic request could not be saved");
    return;
  }
  active_->kernel_->logger().info("shell", "restarting into diagnostics");
  active_->setTaskText("Restarting...");
  delay(150);
  ESP.restart();
}

void DesktopShell::cancelDialogEvent(lv_event_t*) { active_->closeDialog(); }

void DesktopShell::fileEntryEvent(lv_event_t* event) {
  const uint8_t index = static_cast<uint8_t>(
      reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  if (index >= active_->fileEntryCount_) return;
  const StorageEntry& entry = active_->fileEntries_[index];
  if (entry.directory) {
    strlcpy(active_->currentPath_, entry.path, sizeof(active_->currentPath_));
    active_->filePage_ = 0;
    active_->openFiles();
  } else if (StorageService::isImagePath(entry.path)) {
    active_->openImage(entry.path);
  } else {
    active_->showInfoDialog("No application is associated\nwith this file type yet.");
  }
}

void DesktopShell::filesUpEvent(lv_event_t*) {
  active_->parentDirectory();
  active_->openFiles();
}

void DesktopShell::filesPreviousEvent(lv_event_t*) {
  if (active_->filePage_ > 0) --active_->filePage_;
  active_->openFiles();
}

void DesktopShell::filesNextEvent(lv_event_t*) {
  constexpr uint8_t entriesPerPage = StorageService::PAGE_ENTRIES;
  const uint8_t pageCount = active_->fileTotalCount_ == 0
                                ? 1
                                : (active_->fileTotalCount_ + entriesPerPage - 1) /
                                      entriesPerPage;
  if (active_->filePage_ + 1 < pageCount) ++active_->filePage_;
  active_->openFiles();
}

void DesktopShell::imageBackEvent(lv_event_t*) { active_->openFiles(); }

void DesktopShell::setWallpaperEvent(lv_event_t*) {
  if (!active_->settings_.saveWallpaper(active_->selectedImagePath_)) {
    active_->kernel_->faults().report(FaultCode::StorageUnavailable,
                                      "settings",
                                      "wallpaper path could not be saved");
    return;
  }
  active_->applyWallpaper();
  active_->setTaskText("Wallpaper saved");
}

void DesktopShell::clearWallpaperEvent(lv_event_t*) {
  active_->settings_.clearWallpaper();
  active_->removeWallpaper();
  active_->setTaskText("Wallpaper cleared");
}
