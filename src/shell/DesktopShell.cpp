#include "DesktopShell.h"

#include <strings.h>

#include <esp_heap_caps.h>
#include <esp_system.h>
#include <misc/cache/instance/lv_image_cache.h>

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

struct DesktopColorOption {
  uint32_t top;
  uint32_t bottom;
  const char* english;
  const char* russian;
};

constexpr DesktopColorOption DESKTOP_COLORS[] = {
    {COLOR_DESKTOP_TOP, COLOR_DESKTOP_BOTTOM, "Windows blue", "Синий"},
    {0x243447, 0x111827, "Midnight", "Ночной"},
    {0x0F766E, 0x164E63, "Teal", "Бирюзовый"},
    {0x5B3A70, 0x312E81, "Plum", "Сливовый"},
    {0x475569, 0x1E293B, "Slate", "Серый"},
    {0x3F6212, 0x14532D, "Forest", "Лесной"},
};
constexpr uint8_t DESKTOP_COLOR_COUNT =
    sizeof(DESKTOP_COLORS) / sizeof(DESKTOP_COLORS[0]);
constexpr uint32_t SCREEN_SAVER_TIMEOUTS[] = {60000, 120000, 300000, 600000,
                                               1800000};
constexpr uint8_t SCREEN_SAVER_TIMEOUT_COUNT =
    sizeof(SCREEN_SAVER_TIMEOUTS) / sizeof(SCREEN_SAVER_TIMEOUTS[0]);
constexpr uint8_t NOTE_CREATE_INDEX = 0xFF;

uint8_t daysInMonth(int year, int month) {
  static constexpr uint8_t DAYS[] = {31, 28, 31, 30, 31, 30,
                                     31, 31, 30, 31, 30, 31};
  if (month == 2) {
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return leap ? 29 : 28;
  }
  return DAYS[month - 1];
}

const char* const RUSSIAN_KEYBOARD_LOWER[] = {
    "1#", "й", "ц", "у", "к", "е", "н", "г", "ш", "щ", "з", "х", "ъ",
    LV_SYMBOL_BACKSPACE, "\n",
    "ABC", "ф", "ы", "в", "а", "п", "р", "о", "л", "д", "ж", "э",
    LV_SYMBOL_NEW_LINE, "\n",
    "ё", "я", "ч", "с", "м", "и", "т", "ь", "б", "ю", ".", ",", "\n",
    LV_SYMBOL_KEYBOARD, LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""};

const char* const RUSSIAN_KEYBOARD_UPPER[] = {
    "1#", "Й", "Ц", "У", "К", "Е", "Н", "Г", "Ш", "Щ", "З", "Х", "Ъ",
    LV_SYMBOL_BACKSPACE, "\n",
    "abc", "Ф", "Ы", "В", "А", "П", "Р", "О", "Л", "Д", "Ж", "Э",
    LV_SYMBOL_NEW_LINE, "\n",
    "Ё", "Я", "Ч", "С", "М", "И", "Т", "Ь", "Б", "Ю", ".", ",", "\n",
    LV_SYMBOL_KEYBOARD, LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, LV_SYMBOL_OK, ""};

constexpr lv_buttonmatrix_ctrl_t keyboardControl(uint16_t flags,
                                                  uint8_t width) {
  return static_cast<lv_buttonmatrix_ctrl_t>(flags | width);
}

const lv_buttonmatrix_ctrl_t RUSSIAN_KEYBOARD_CONTROLS[] = {
    // Mode, 12 letters, Backspace.
    keyboardControl(LV_KEYBOARD_CTRL_BUTTON_FLAGS, 4),
    keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2), keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2), keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2), keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2), keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2), keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2), keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_CHECKED, 5),
    // Shift, 11 letters, Enter.
    keyboardControl(LV_KEYBOARD_CTRL_BUTTON_FLAGS, 4),
    keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2), keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2), keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2), keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2), keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2), keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_CHECKED, 5),
    // Ё, 9 letters and punctuation.
    keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2), keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2), keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2), keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2), keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2), keyboardControl(LV_BUTTONMATRIX_CTRL_POPOVER, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_CHECKED, 2),
    keyboardControl(LV_BUTTONMATRIX_CTRL_CHECKED, 2),
    // Hide, cursor left, visible space area, cursor right, Done.
    keyboardControl(LV_KEYBOARD_CTRL_BUTTON_FLAGS, 4),
    keyboardControl(LV_BUTTONMATRIX_CTRL_CHECKED, 3),
    keyboardControl(0, 10),
    keyboardControl(LV_BUTTONMATRIX_CTRL_CHECKED, 3),
    keyboardControl(LV_KEYBOARD_CTRL_BUTTON_FLAGS, 4)};
static_assert(sizeof(RUSSIAN_KEYBOARD_CONTROLS) /
                  sizeof(RUSSIAN_KEYBOARD_CONTROLS[0]) ==
              44,
              "Russian keyboard map and control map must stay aligned");

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
  language_ = settings_.loadLanguage();
  desktopColor_ = settings_.loadDesktopColor();
  if (desktopColor_ >= DESKTOP_COLOR_COUNT) desktopColor_ = 0;
  screenSaverEnabled_ = settings_.loadScreenSaverEnabled();
  screenSaverTimeoutIndex_ = settings_.loadScreenSaverTimeout();
  if (screenSaverTimeoutIndex_ >= SCREEN_SAVER_TIMEOUT_COUNT)
    screenSaverTimeoutIndex_ = 2;
  const uint8_t savedScreenSaverMode = settings_.loadScreenSaverMode();
  screenSaverMode_ = savedScreenSaverMode <=
                             static_cast<uint8_t>(ScreenSaverMode::Starfield)
                         ? static_cast<ScreenSaverMode>(savedScreenSaverMode)
                         : ScreenSaverMode::Clock;
  settings_.loadScreenSaverImage(screenSaverImagePath_,
                                 sizeof(screenSaverImagePath_));
  localization_.setLanguage(language_);
  dateTime_.begin(settings_);
  if (!port_.begin(rotation180_, uiFont())) {
    kernel_->faults().report(FaultCode::InternalError, "shell",
                             "LVGL port initialization failed");
    return false;
  }

  storage_.begin(kernel_->events(), kernel_->logger());
  notes_.begin(storage_);
  storage_.registerLvglDriver();
  wallpaperService_.begin(storage_, kernel_->logger());
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
    wallpaperService_.invalidateCache();
    if (previousStorageMounted_)
      applyWallpaper();
    else {
      hideScreenSaver();
      if (!noteEditorOpen_) closeWindow();
      removeWallpaper();
      setTaskText(tr("SD card removed", "SD-карта извлечена"));
      if (noteEditorOpen_)
        showInfoDialog(tr("SD card removed. The note stays open, but it cannot "
                          "be saved until the card returns.",
                          "SD-карта извлечена. Заметка останется открытой, но "
                          "сохранение невозможно до возврата карты."));
    }
  }
  if (calibration_.active()) updateCalibration();
  updateClock();
  updateScreenSaver();
  delay(2);
}

void DesktopShell::setFullscreenApplicationActive(bool active) {
  fullscreenApplicationActive_ = active;
  if (active) hideScreenSaver();
  lv_display_trigger_activity(nullptr);
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

lv_obj_t* DesktopShell::createDesktopShortcut(const char* icon,
                                               const char* text,
                                               uint32_t iconColor, int16_t x,
                                               int16_t y, ShellAppId app) {
  lv_obj_t* shortcut = lv_button_create(screen_);
  lv_obj_set_pos(shortcut, x, y);
  lv_obj_set_size(shortcut, 68, 72);
  lv_obj_set_style_bg_opa(shortcut, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(shortcut, lv_color_hex(0xB8E3FF),
                            LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(shortcut, LV_OPA_40, LV_STATE_PRESSED);
  lv_obj_set_style_border_width(shortcut, 0, 0);
  lv_obj_set_style_shadow_width(shortcut, 0, 0);
  lv_obj_set_style_radius(shortcut, 2, 0);
  lv_obj_set_style_pad_all(shortcut, 0, 0);
  lv_obj_add_event_cb(
      shortcut, appButtonEvent, LV_EVENT_CLICKED,
      reinterpret_cast<void*>(static_cast<uintptr_t>(app)));

  lv_obj_t* iconLabel = lv_label_create(shortcut);
  lv_label_set_text(iconLabel, icon);
  lv_obj_set_style_text_font(iconLabel, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(iconLabel, lv_color_hex(iconColor), 0);
  lv_obj_set_pos(iconLabel, 0, 3);
  lv_obj_set_width(iconLabel, 68);
  lv_obj_set_style_text_align(iconLabel, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t* textShadow = lv_label_create(shortcut);
  lv_label_set_text(textShadow, text);
  lv_obj_set_style_text_font(textShadow, uiSmallFont(), 0);
  lv_obj_set_style_text_color(textShadow, lv_color_black(), 0);
  lv_obj_set_style_text_opa(textShadow, LV_OPA_70, 0);
  lv_obj_set_style_text_align(textShadow, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(textShadow, 2, 41);
  lv_obj_set_width(textShadow, 66);

  lv_obj_t* textLabel = lv_label_create(shortcut);
  lv_label_set_text(textLabel, text);
  lv_obj_set_style_text_font(textLabel, uiSmallFont(), 0);
  lv_obj_set_style_text_color(textLabel, lv_color_white(), 0);
  lv_obj_set_style_text_align(textLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(textLabel, 1, 40);
  lv_obj_set_width(textLabel, 66);
  return shortcut;
}

lv_obj_t* DesktopShell::createSettingsRow(
    lv_obj_t* parent, const char* icon, const char* title, const char* summary,
    int16_t y, lv_event_cb_t callback) {
  lv_obj_t* row = lv_button_create(parent);
  lv_obj_set_pos(row, 5, y);
  lv_obj_set_size(row, 296, 48);
  lv_obj_set_style_bg_color(row, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_color(row, lv_color_hex(0xDDEEFF), LV_STATE_PRESSED);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_color(row, lv_color_hex(0xD0D5DA), 0);
  lv_obj_set_style_radius(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_add_event_cb(row, callback, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* iconLabel = lv_label_create(row);
  lv_label_set_text(iconLabel, icon);
  lv_obj_set_style_text_color(iconLabel, lv_color_hex(COLOR_ACCENT), 0);
  lv_obj_set_pos(iconLabel, 10, 15);
  lv_obj_set_width(iconLabel, 26);
  lv_obj_set_style_text_align(iconLabel, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t* titleLabel = lv_label_create(row);
  lv_label_set_text(titleLabel, title);
  lv_obj_set_pos(titleLabel, 45, 5);
  lv_obj_set_style_text_color(titleLabel, lv_color_hex(0x202020), 0);

  lv_obj_t* summaryLabel = lv_label_create(row);
  lv_label_set_text(summaryLabel, summary);
  lv_obj_set_pos(summaryLabel, 45, 25);
  lv_obj_set_width(summaryLabel, 218);
  lv_label_set_long_mode(summaryLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(summaryLabel, uiSmallFont(), 0);
  lv_obj_set_style_text_color(summaryLabel, lv_color_hex(0x60666C), 0);

  lv_obj_t* arrow = lv_label_create(row);
  lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
  lv_obj_set_pos(arrow, 274, 15);
  lv_obj_set_style_text_color(arrow, lv_color_hex(0x60666C), 0);
  return row;
}

lv_obj_t* DesktopShell::createColorChoice(lv_obj_t* parent,
                                          uint8_t colorIndex, int16_t x,
                                          int16_t y) {
  const DesktopColorOption& option = DESKTOP_COLORS[colorIndex];
  lv_obj_t* choice = lv_button_create(parent);
  lv_obj_set_pos(choice, x, y);
  lv_obj_set_size(choice, 92, 44);
  lv_obj_set_style_bg_color(choice, lv_color_hex(option.top), 0);
  lv_obj_set_style_bg_grad_color(choice, lv_color_hex(option.bottom), 0);
  lv_obj_set_style_bg_grad_dir(choice, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_radius(choice, 3, 0);
  lv_obj_set_style_border_width(choice,
                                desktopColor_ == colorIndex ? 3 : 1, 0);
  lv_obj_set_style_border_color(
      choice,
      lv_color_hex(desktopColor_ == colorIndex ? 0xFFFFFF : 0xAAB2BA), 0);
  lv_obj_set_style_pad_all(choice, 2, 0);
  lv_obj_add_event_cb(
      choice, desktopColorEvent, LV_EVENT_CLICKED,
      reinterpret_cast<void*>(static_cast<uintptr_t>(colorIndex)));

  lv_obj_t* label = lv_label_create(choice);
  lv_label_set_text(label, tr(option.english, option.russian));
  lv_obj_set_style_text_font(label, uiSmallFont(), 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(label);
  return choice;
}

void DesktopShell::configureKeyboard(lv_obj_t* keyboard) {
  lv_obj_set_style_pad_all(keyboard, 2, 0);
  lv_obj_set_style_pad_row(keyboard, 2, 0);
  lv_obj_set_style_pad_column(keyboard, 2, 0);
  lv_obj_set_style_bg_color(keyboard, lv_color_hex(0xD7DCE1), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(keyboard, lv_color_hex(0xF8F9FA), LV_PART_ITEMS);
  lv_obj_set_style_bg_color(keyboard, lv_color_hex(COLOR_ACCENT),
                            LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(keyboard, lv_color_hex(0xC8E4FA),
                            LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_text_color(keyboard, lv_color_hex(0x202020), LV_PART_ITEMS);
  lv_obj_set_style_text_color(keyboard, lv_color_white(),
                              LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_width(keyboard, 1, LV_PART_ITEMS);
  lv_obj_set_style_border_color(keyboard, lv_color_hex(0xAAB2BA),
                                LV_PART_ITEMS);
  lv_obj_set_style_text_font(keyboard, uiSmallFont(), LV_PART_ITEMS);
  if (language_ == SystemLanguage::Russian) {
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER,
                        RUSSIAN_KEYBOARD_LOWER, RUSSIAN_KEYBOARD_CONTROLS);
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_UPPER,
                        RUSSIAN_KEYBOARD_UPPER, RUSSIAN_KEYBOARD_CONTROLS);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
  }
}

void DesktopShell::buildDesktop() {
  screen_ = lv_screen_active();
  configurePanel(screen_, COLOR_DESKTOP_TOP);
  applyDesktopColor();
  applyWallpaper();

  lv_obj_t* brand = lv_label_create(screen_);
  lv_label_set_text(brand, "OSEsp32");
  lv_obj_set_pos(brand, 218, 8);
  lv_obj_set_style_text_color(brand, lv_color_hex(0xBFE9FF), 0);

  createDesktopShortcut(LV_SYMBOL_DIRECTORY, tr("Files", "Файлы"), 0xFFD45A, 5, 28,
                        ShellAppId::Files);
  createDesktopShortcut(LV_SYMBOL_SETTINGS, tr("Settings", "Настройки"), 0xE9EEF2, 79, 28,
                        ShellAppId::Settings);
  createDesktopShortcut(LV_SYMBOL_LIST, tr("System\nInfo", "Сведения"), 0x79D1FF, 153, 28,
                        ShellAppId::SystemInfo);
  createDesktopShortcut(LV_SYMBOL_EDIT, tr("Notes", "Заметки"), 0xFFF2A8, 227, 28,
                        ShellAppId::Notes);

  buildTaskbar();
  buildStartMenu();
  updateClock();
}

void DesktopShell::buildTaskbar() {
  lv_obj_t* taskbar = lv_obj_create(screen_);
  lv_obj_set_pos(taskbar, 0, TASKBAR_Y);
  lv_obj_set_size(taskbar, board::SCREEN_WIDTH, TASKBAR_HEIGHT);
  configurePanel(taskbar, COLOR_TASKBAR);

  createButton(taskbar, tr("START", "ПУСК"), 3, 3, 66, 30, startButtonEvent);
  taskLabel_ = lv_label_create(taskbar);
  lv_label_set_text(taskLabel_, tr("Desktop", "Рабочий стол"));
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
  lv_label_set_text(title, tr("OSEsp32 applications", "Приложения OSEsp32"));
  lv_obj_set_pos(title, 8, 7);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);

  const char* names[] = {tr("Files", "Файлы"), tr("Settings", "Настройки"),
                         tr("System Info", "Сведения о системе"),
                         tr("Notes", "Заметки"),
                         tr("About", "О системе")};
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
  noteTitleArea_ = nullptr;
  noteBodyArea_ = nullptr;
  noteKeyboard_ = nullptr;
  noteHideKeyboardButton_ = nullptr;
  dateTimeContent_ = nullptr;
  noteEditorOpen_ = false;
  noteKeyboardVisible_ = false;
  fullscreenApplicationActive_ = false;
  setTaskText(tr("Desktop", "Рабочий стол"));
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
  lv_label_set_text(message, tr(
      "Restart into hardware diagnostics?\nThe next boot returns to the shell.",
      "Перейти к диагностике оборудования?\nСледующая загрузка вернёт оболочку."));
  lv_obj_set_pos(message, 12, 12);
  lv_obj_set_style_text_color(message, lv_color_hex(0x202020), 0);
  createButton(dialog_, tr("CANCEL", "ОТМЕНА"), 18, 75, 92, 32, cancelDialogEvent);
  createButton(dialog_, tr("RESTART", "ПЕРЕЗАПУСК"), 139, 75, 92, 32,
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
  createButton(dialog_, tr("OK", "ОК"), 79, 68, 92, 30, cancelDialogEvent);
  lv_obj_move_foreground(dialog_);
}

void DesktopShell::openApp(ShellAppId app) {
  kernel_->events().publish(SystemEventType::ShellApplicationOpened,
                            static_cast<uint32_t>(app));
  switch (app) {
    case ShellAppId::Files: openFiles(); break;
    case ShellAppId::Settings: openSettings(); break;
    case ShellAppId::SystemInfo: openSystemInfo(); break;
    case ShellAppId::Notes: openNotes(); break;
    case ShellAppId::About: openAbout(); break;
  }
}

void DesktopShell::openFiles() {
  lv_obj_t* content = createWindow(tr("Files", "Файлы"));
  if (!storage_.mounted()) {
    lv_obj_t* label = lv_label_create(content);
    lv_label_set_text(label, tr(
        "SD card is not available.\nInsert a FAT32 card and reopen Files.",
        "SD-карта недоступна.\nВставьте FAT32-карту и откройте Файлы."));
    lv_obj_set_pos(label, 14, 24);
    return;
  }

  constexpr uint8_t entriesPerPage = StorageService::PAGE_ENTRIES;
  if (!storage_.listDirectoryPage(currentPath_, filePage_ * entriesPerPage,
                                  fileEntries_, entriesPerPage,
                                  fileEntryCount_, fileTotalCount_)) {
    lv_obj_t* label = lv_label_create(content);
    lv_label_set_text(label, tr("Unable to read this directory.",
                                "Не удалось прочитать эту папку."));
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

  createButton(content, tr("UP", "ВВЕРХ"), 5, 132, 58, 27, filesUpEvent);
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
    showInfoDialog(tr("Image path is too long.",
                      "Слишком длинный путь к изображению."));
    return;
  }
  lv_image_header_t header;
  if (lv_image_decoder_get_info(selectedImageLvPath_, &header) != LV_RESULT_OK) {
    const char* extension = strrchr(path, '.');
    if (extension && (!strcasecmp(extension, ".jpg") ||
                      !strcasecmp(extension, ".jpeg")))
      showInfoDialog(tr("Unsupported JPEG encoding.\nTry baseline (not progressive) JPEG.",
                        "Кодирование JPEG не поддерживается.\nИспользуйте baseline, не progressive."));
    else
      showInfoDialog(tr("Unsupported BMP encoding.\nUse uncompressed 16/24/32-bit BMP.",
                        "Формат BMP не поддерживается.\nНужен несжатый BMP 16/24/32 бит."));
    return;
  }

  lv_obj_t* content = createWindow(tr("Image Viewer", "Просмотр изображения"));
  lv_obj_t* image = lv_image_create(content);
  lv_obj_set_pos(image, 3, 2);
  lv_obj_set_size(image, 300, 120);
  lv_obj_set_style_bg_color(image, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(image, LV_OPA_COVER, 0);
  lv_image_set_inner_align(image, LV_IMAGE_ALIGN_CENTER);
  lv_image_set_src(image, selectedImageLvPath_);
  createButton(content, tr("FILES", "ФАЙЛЫ"), 5, 127, 58, 30,
               imageBackEvent);
  createButton(content, tr("WALLPAPER", "ОБОИ"), 68, 127, 112, 30,
               setWallpaperEvent);
  createButton(content, tr("SAVER", "ЗАСТАВКА"), 185, 127, 114, 30,
               setScreenSaverImageEvent);
}

void DesktopShell::openSettings() {
  lv_obj_t* content = createWindow(tr("Settings", "Параметры"));
  lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(content, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
  createSettingsRow(content, LV_SYMBOL_IMAGE, tr("Display", "Экран"),
                    tr("Brightness, rotation and wallpaper",
                       "Яркость, поворот и обои"),
                    3, settingsDisplayEvent);
  createSettingsRow(content, "A", tr("Language", "Язык"),
                    language_ == SystemLanguage::Russian ? "Русский"
                                                         : "English",
                    54, settingsLanguageEvent);
  createSettingsRow(content, LV_SYMBOL_EDIT, tr("Touch", "Сенсор"),
                    tr("Calibration and touch coordinates",
                       "Калибровка и координаты"),
                    105, settingsTouchEvent);
  char dateSummary[40];
  dateTime_.formatDateTime(dateSummary, sizeof(dateSummary));
  createSettingsRow(content, LV_SYMBOL_REFRESH,
                    tr("Date & time", "Дата и время"),
                    dateTime_.valid() ? dateSummary
                                      : tr("Not set", "Не установлены"),
                    156, settingsDateTimeEvent);
  createSettingsRow(content, LV_SYMBOL_IMAGE,
                    tr("Screen saver", "Заставка"),
                    screenSaverEnabled_ ? tr("On", "Включена")
                                        : tr("Off", "Выключена"),
                    207, settingsScreenSaverEvent);
}

void DesktopShell::openDisplaySettings() {
  lv_obj_t* content = createWindow(tr("Display settings", "Параметры экрана"));
  createButton(content, tr("BACK", "НАЗАД"), 5, 4, 62, 27,
               settingsBackEvent);
  lv_obj_t* heading = lv_label_create(content);
  lv_label_set_text(heading, tr("Brightness", "Яркость"));
  lv_obj_set_pos(heading, 78, 10);

  lv_obj_t* slider = lv_slider_create(content);
  lv_obj_set_pos(slider, 10, 45);
  lv_obj_set_size(slider, 286, 16);
  lv_slider_set_range(slider, 25, 255);
  lv_slider_set_value(slider, port_.displayDriver().getBrightness(),
                      LV_ANIM_OFF);
  lv_obj_add_event_cb(slider, brightnessEvent, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(slider, brightnessSaveEvent, LV_EVENT_RELEASED, nullptr);

  createButton(content,
               rotation180_ ? tr("ROTATE TO 0", "ПОВЕРНУТЬ НА 0")
                            : tr("ROTATE TO 180", "ПОВЕРНУТЬ НА 180"),
               10, 75, 286, 32, rotationEvent);
  createButton(content, tr("DESKTOP COLOR", "ЦВЕТ СТОЛА"),
               10, 116, 138, 32, desktopColorSettingsEvent);
  createButton(content, tr("CLEAR WALLPAPER", "УБРАТЬ ОБОИ"),
               157, 116, 139, 32, clearWallpaperEvent);
}

void DesktopShell::openDesktopColorSettings() {
  lv_obj_t* content = createWindow(tr("Desktop color", "Цвет рабочего стола"));
  createButton(content, tr("BACK", "НАЗАД"), 5, 4, 62, 27,
               settingsDisplayEvent);
  lv_obj_t* note = lv_label_create(content);
  lv_label_set_text(note, tr("Used when wallpaper is off",
                             "Виден, когда обои отключены"));
  lv_obj_set_style_text_font(note, uiSmallFont(), 0);
  lv_obj_set_pos(note, 77, 10);

  for (uint8_t index = 0; index < DESKTOP_COLOR_COUNT; ++index) {
    const int16_t x = 5 + (index % 3) * 99;
    const int16_t y = 39 + (index / 3) * 53;
    createColorChoice(content, index, x, y);
  }
}

void DesktopShell::openLanguageSettings() {
  lv_obj_t* content = createWindow(tr("Language", "Язык"));
  createButton(content, tr("BACK", "НАЗАД"), 5, 4, 62, 27,
               settingsBackEvent);
  lv_obj_t* note = lv_label_create(content);
  lv_label_set_text(note, tr("Choose interface language",
                             "Выберите язык интерфейса"));
  lv_obj_set_pos(note, 78, 10);
  createSettingsRow(content, "EN", "English",
                    language_ == SystemLanguage::English
                        ? tr("Current language", "Текущий язык")
                        : tr("Available", "Доступен"),
                    39, languageEnglishEvent);
  createSettingsRow(content, "RU", "Русский",
                    language_ == SystemLanguage::Russian
                        ? tr("Current language", "Текущий язык")
                        : tr("Available", "Доступен"),
                    92, languageRussianEvent);
}

void DesktopShell::openTouchSettings() {
  lv_obj_t* content = createWindow(tr("Touch settings", "Параметры сенсора"));
  createButton(content, tr("BACK", "НАЗАД"), 5, 4, 62, 27,
               settingsBackEvent);
  createButton(content, tr("CALIBRATE", "КАЛИБРОВАТЬ"), 10, 40, 138, 34,
               calibrateEvent);
  createButton(content, tr("RESET + CALIBRATE", "СБРОС + КАЛИБР."),
               157, 40, 139, 34, resetCalibrationEvent);

  const TouchCalibration& calibration = port_.touchDriver().calibration();
  char details[160];
  if (language_ == SystemLanguage::Russian) {
    snprintf(details, sizeof(details),
             "Калибровка: %s\nX %u..%u%s   Y %u..%u%s",
             calibration.stored ? "сохранена" : "по умолчанию",
             calibration.rawXMin, calibration.rawXMax,
             calibration.invertX ? " обр" : "", calibration.rawYMin,
             calibration.rawYMax, calibration.invertY ? " обр" : "");
  } else {
    snprintf(details, sizeof(details),
             "Calibration: %s\nX %u..%u%s   Y %u..%u%s",
             calibration.stored ? "saved" : "board defaults",
             calibration.rawXMin, calibration.rawXMax,
             calibration.invertX ? " inv" : "", calibration.rawYMin,
             calibration.rawYMax, calibration.invertY ? " inv" : "");
  }
  lv_obj_t* detailLabel = lv_label_create(content);
  lv_label_set_text(detailLabel, details);
  lv_obj_set_pos(detailLabel, 10, 92);
}

void DesktopShell::prepareDateTimeEdit() {
  if (!dateTime_.localTime(pendingDateTime_)) {
    memset(&pendingDateTime_, 0, sizeof(pendingDateTime_));
    pendingDateTime_.tm_year = 126;
    pendingDateTime_.tm_mon = 0;
    pendingDateTime_.tm_mday = 1;
    pendingDateTime_.tm_hour = 12;
  }
  pendingTimezoneMinutes_ = dateTime_.timezoneMinutes();
  dateTimeEditPrepared_ = true;
}

void DesktopShell::openDateTimeSettings() {
  if (!dateTimeEditPrepared_) prepareDateTimeEdit();
  lv_obj_t* content = createWindow(tr("Date & time", "Дата и время"));
  dateTimeContent_ = content;
  lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(content, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
  createButton(content, tr("BACK", "НАЗАД"), 5, 4, 62, 27,
               settingsBackEvent);
  lv_obj_t* note = lv_label_create(content);
  lv_label_set_text(note, tr("Manual now; Internet sync later",
                             "Сейчас вручную; позже через интернет"));
  lv_obj_set_style_text_font(note, uiSmallFont(), 0);
  lv_obj_set_pos(note, 76, 10);

  const char* namesEn[] = {"Year", "Month", "Day", "Hour", "Minute", "UTC"};
  const char* namesRu[] = {"Год", "Месяц", "День", "Час", "Минута", "UTC"};
  const int values[] = {pendingDateTime_.tm_year + 1900,
                        pendingDateTime_.tm_mon + 1,
                        pendingDateTime_.tm_mday,
                        pendingDateTime_.tm_hour,
                        pendingDateTime_.tm_min,
                        pendingTimezoneMinutes_};
  for (uint8_t field = 0; field < 6; ++field) {
    const int16_t y = 40 + field * 39;
    lv_obj_t* label = lv_label_create(content);
    lv_label_set_text(label, tr(namesEn[field], namesRu[field]));
    lv_obj_set_pos(label, 12, y + 9);
    createButton(content, "-", 112, y, 38, 32, dateTimeAdjustEvent,
                 reinterpret_cast<void*>(static_cast<uintptr_t>(field * 2)));
    char value[20];
    if (field == 5) {
      const int absoluteMinutes = abs(values[field]);
      snprintf(value, sizeof(value), "UTC%c%02d:%02d",
               values[field] < 0 ? '-' : '+', absoluteMinutes / 60,
               absoluteMinutes % 60);
    } else
      snprintf(value, sizeof(value), field == 0 ? "%04d" : "%02d",
               values[field]);
    lv_obj_t* valueLabel = lv_label_create(content);
    lv_label_set_text(valueLabel, value);
    lv_obj_set_pos(valueLabel, 158, y + 9);
    lv_obj_set_width(valueLabel, 88);
    lv_obj_set_style_text_align(valueLabel, LV_TEXT_ALIGN_CENTER, 0);
    createButton(content, "+", 256, y, 38, 32, dateTimeAdjustEvent,
                 reinterpret_cast<void*>(
                     static_cast<uintptr_t>(field * 2 + 1)));
  }
  createButton(content, tr("SAVE DATE AND TIME", "СОХРАНИТЬ ДАТУ И ВРЕМЯ"),
               12, 280, 282, 34, dateTimeSaveEvent);
  lv_obj_update_layout(content);
  lv_obj_scroll_to_y(content, dateTimeScrollY_, LV_ANIM_OFF);
}

void DesktopShell::openScreenSaverSettings() {
  lv_obj_t* content = createWindow(tr("Screen saver", "Заставка"));
  lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(content, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
  createButton(content, tr("BACK", "НАЗАД"), 5, 4, 62, 27,
               settingsBackEvent);

  createSettingsRow(content, LV_SYMBOL_POWER,
                    tr("Screen saver", "Заставка"),
                    screenSaverEnabled_ ? tr("On", "Включена")
                                        : tr("Off", "Выключена"),
                    39, screenSaverEnabledEvent);
  char timeout[32];
  const uint32_t minutes =
      SCREEN_SAVER_TIMEOUTS[screenSaverTimeoutIndex_] / 60000;
  snprintf(timeout, sizeof(timeout), language_ == SystemLanguage::Russian
             ? "%lu мин. бездействия" : "%lu min inactive",
           static_cast<unsigned long>(minutes));
  createSettingsRow(content, LV_SYMBOL_REFRESH,
                    tr("Start after", "Запускать через"), timeout,
                    90, screenSaverTimeoutEvent);

  createButton(content, LV_SYMBOL_LEFT, 12, 141, 42, 43,
               screenSaverModeEvent, nullptr);
  lv_obj_t* modePanel = lv_obj_create(content);
  lv_obj_set_pos(modePanel, 59, 141);
  lv_obj_set_size(modePanel, 188, 43);
  configurePanel(modePanel, 0xFFFFFF, 5);
  lv_obj_set_style_border_width(modePanel, 1, 0);
  lv_obj_set_style_border_color(modePanel, lv_color_hex(0xC8CDD2), 0);
  lv_obj_t* modeLabel = lv_label_create(modePanel);
  const char* modeText = tr("Clock", "Часы");
  if (screenSaverMode_ == ScreenSaverMode::Picture)
    modeText = tr("Picture", "Картинка");
  else if (screenSaverMode_ == ScreenSaverMode::Starfield)
    modeText = tr("Starfield", "Звёздное поле");
  lv_label_set_text(modeLabel, modeText);
  lv_obj_center(modeLabel);
  createButton(content, LV_SYMBOL_RIGHT, 252, 141, 42, 43,
               screenSaverModeEvent,
               reinterpret_cast<void*>(static_cast<uintptr_t>(1)));

  const char* imageName = strrchr(screenSaverImagePath_, '/');
  createSettingsRow(content, LV_SYMBOL_IMAGE,
                    tr("Screen saver picture", "Картинка заставки"),
                    screenSaverImagePath_[0]
                        ? (imageName ? imageName + 1 : screenSaverImagePath_)
                        : tr("Not selected", "Не выбрана"),
                    192, screenSaverChooseImageEvent);
  if (screenSaverImagePath_[0])
    createSettingsRow(content, LV_SYMBOL_TRASH,
                      tr("Clear background image", "Убрать фоновую картинку"),
                      tr("Picture mode will be black", "Режим будет чёрным"),
                      243, screenSaverClearImageEvent);

  lv_obj_t* help = lv_label_create(content);
  lv_label_set_text(help, tr(
      "Choose a picture in Files, open it, then tap SAVER.",
      "Выберите картинку в Файлах, откройте её и нажмите ЗАСТАВКА."));
  lv_obj_set_style_text_font(help, uiSmallFont(), 0);
  lv_obj_set_width(help, 280);
  lv_label_set_long_mode(help, LV_LABEL_LONG_WRAP);
  lv_obj_set_pos(help, 12, screenSaverImagePath_[0] ? 298 : 247);
}

void DesktopShell::openSystemInfo() {
  lv_obj_t* content = createWindow(tr("System Info", "Сведения о системе"));
  const MemorySnapshot memory = kernel_->monitor().sample();
  char info[256];
  snprintf(info, sizeof(info), language_ == SystemLanguage::Russian
           ? "ESP32  %u МГц   Flash %u МиБ\n"
             "Свободно %u КиБ   минимум %u КиБ\n"
             "Макс. блок %u КиБ\n"
             "События %lu   пропущено %lu   сбои %lu"
           : "ESP32  %u MHz   Flash %u MiB\n"
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
  createButton(content, tr("HARDWARE DIAGNOSTICS", "ДИАГНОСТИКА ОБОРУДОВАНИЯ"), 42, 104, 220, 36,
               diagnosticsEvent);
}

void DesktopShell::openNotes() {
  noteEditorOpen_ = false;
  noteDirty_ = false;
  noteCount_ = notes_.list(noteSummaries_, NotesService::MAX_NOTES);
  lv_obj_t* content = createWindow(tr("Notes", "Заметки"));
  if (!storage_.mounted()) {
    lv_obj_t* label = lv_label_create(content);
    lv_label_set_text(label, tr(
        "Notes need an SD card.\nInsert the card and reopen Notes.",
        "Для заметок нужна SD-карта.\nВставьте карту и откройте Заметки."));
    lv_obj_set_pos(label, 14, 24);
    return;
  }
  lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(content, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

  for (uint8_t cardIndex = 0; cardIndex <= noteCount_; ++cardIndex) {
    const int16_t x = 4 + (cardIndex % 3) * 99;
    const int16_t y = 4 + (cardIndex / 3) * 99;
    lv_obj_t* card = lv_button_create(content);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, 92, 91);
    lv_obj_set_style_radius(card, 9, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFDF2), 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xE8F3FF), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xC8C5B8), 0);
    lv_obj_set_style_shadow_width(card, 3, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    const uint8_t noteIndex = cardIndex == 0 ? NOTE_CREATE_INDEX
                                             : cardIndex - 1;
    lv_obj_add_event_cb(
        card, noteCardEvent, LV_EVENT_CLICKED,
        reinterpret_cast<void*>(static_cast<uintptr_t>(noteIndex)));

    if (cardIndex == 0) {
      lv_obj_t* plus = lv_label_create(card);
      lv_label_set_text(plus, "+");
      lv_obj_set_style_text_font(plus, &lv_font_montserrat_28, 0);
      lv_obj_set_style_text_color(plus, lv_color_hex(COLOR_ACCENT), 0);
      lv_obj_set_pos(plus, 0, 13);
      lv_obj_set_width(plus, 92);
      lv_obj_set_style_text_align(plus, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_t* title = lv_label_create(card);
      lv_label_set_text(title, tr("New note", "Новая заметка"));
      lv_obj_set_style_text_font(title, uiSmallFont(), 0);
      lv_obj_set_style_text_color(title, lv_color_hex(0x303030), 0);
      lv_obj_set_pos(title, 5, 67);
      lv_obj_set_width(title, 82);
      lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
      lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
      continue;
    }

    const NoteSummary& summary = noteSummaries_[noteIndex];
    lv_obj_t* preview = lv_label_create(card);
    lv_label_set_text(preview,
                      summary.preview[0] ? summary.preview : tr("Empty note", "Пустая заметка"));
    lv_obj_set_style_text_font(preview, uiSmallFont(), 0);
    lv_obj_set_style_text_color(preview, lv_color_hex(0x505050), 0);
    lv_obj_set_pos(preview, 7, 7);
    lv_obj_set_size(preview, 78, 51);
    lv_label_set_long_mode(preview, LV_LABEL_LONG_CLIP);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title,
                      summary.title[0] ? summary.title : tr("Untitled", "Без названия"));
    lv_obj_set_style_text_font(title, uiSmallFont(), 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x202020), 0);
    lv_obj_set_pos(title, 7, 67);
    lv_obj_set_width(title, 78);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
  }
}

void DesktopShell::openNoteEditor(const char* path) {
  closeWindow();
  strlcpy(notePath_, path ? path : "", sizeof(notePath_));
  noteTitle_[0] = '\0';
  noteBody_[0] = '\0';
  if (path && !notes_.load(path, noteTitle_, sizeof(noteTitle_), noteBody_,
                           sizeof(noteBody_))) {
    openNotes();
    showInfoDialog(tr("Could not open this note.",
                      "Не удалось открыть заметку."));
    return;
  }

  window_ = lv_obj_create(screen_);
  lv_obj_set_pos(window_, 0, 0);
  lv_obj_set_size(window_, board::SCREEN_WIDTH, board::SCREEN_HEIGHT);
  configurePanel(window_, 0xFFFFFF);
  lv_obj_move_foreground(window_);

  lv_obj_t* toolbar = lv_obj_create(window_);
  lv_obj_set_pos(toolbar, 0, 0);
  lv_obj_set_size(toolbar, 320, 32);
  configurePanel(toolbar, COLOR_TITLE);
  createButton(toolbar, LV_SYMBOL_LEFT, 3, 2, 40, 28, noteBackEvent);
  lv_obj_t* caption = lv_label_create(toolbar);
  lv_label_set_text(caption, tr("Note", "Заметка"));
  lv_obj_set_pos(caption, 51, 8);
  lv_obj_set_style_text_color(caption, lv_color_white(), 0);
  noteHideKeyboardButton_ = createButton(
      toolbar, LV_SYMBOL_KEYBOARD, 220, 2, 43, 28,
      noteHideKeyboardEvent);
  createButton(toolbar, LV_SYMBOL_SAVE, 268, 2, 49, 28, noteSaveEvent);

  noteTitleArea_ = lv_textarea_create(window_);
  lv_obj_set_pos(noteTitleArea_, 5, 34);
  lv_obj_set_size(noteTitleArea_, 310, 34);
  lv_textarea_set_one_line(noteTitleArea_, true);
  lv_textarea_set_max_length(noteTitleArea_,
                             NotesService::MAX_TITLE_CHARACTERS);
  lv_textarea_set_cursor_click_pos(noteTitleArea_, true);
  lv_textarea_set_placeholder_text(noteTitleArea_,
                                   tr("Title", "Заголовок"));
  lv_textarea_set_text(noteTitleArea_, noteTitle_);
  lv_obj_set_style_text_font(noteTitleArea_, &osesp32_font_16_bold, 0);
  lv_obj_set_style_border_width(noteTitleArea_, 0, 0);
  lv_obj_set_style_radius(noteTitleArea_, 0, 0);
  lv_obj_set_style_pad_left(noteTitleArea_, 7, 0);
  lv_obj_set_style_pad_right(noteTitleArea_, 7, 0);

  noteBodyArea_ = lv_textarea_create(window_);
  lv_obj_set_pos(noteBodyArea_, 5, 69);
  lv_obj_set_size(noteBodyArea_, 310, 56);
  lv_textarea_set_one_line(noteBodyArea_, false);
  lv_textarea_set_max_length(noteBodyArea_,
                             NotesService::MAX_BODY_CHARACTERS);
  lv_textarea_set_cursor_click_pos(noteBodyArea_, true);
  lv_textarea_set_placeholder_text(noteBodyArea_,
                                   tr("Write a note...", "Текст заметки..."));
  lv_textarea_set_text(noteBodyArea_, noteBody_);
  lv_obj_set_scrollbar_mode(noteBodyArea_, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_border_width(noteBodyArea_, 0, 0);
  lv_obj_set_style_radius(noteBodyArea_, 0, 0);
  lv_obj_set_style_pad_left(noteBodyArea_, 7, 0);
  lv_obj_set_style_pad_right(noteBodyArea_, 7, 0);

  noteKeyboard_ = lv_keyboard_create(window_);
  lv_obj_set_pos(noteKeyboard_, 3, 128);
  lv_obj_set_size(noteKeyboard_, 314, 109);
  configureKeyboard(noteKeyboard_);
  lv_obj_add_event_cb(noteKeyboard_, noteHideKeyboardEvent, LV_EVENT_CANCEL,
                      nullptr);
  lv_keyboard_set_textarea(noteKeyboard_, noteTitleArea_);

  lv_obj_add_event_cb(noteTitleArea_, noteTextChangedEvent,
                      LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(noteBodyArea_, noteTextChangedEvent,
                      LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(noteTitleArea_, noteTextFocusedEvent, LV_EVENT_FOCUSED,
                      nullptr);
  lv_obj_add_event_cb(noteBodyArea_, noteTextFocusedEvent, LV_EVENT_FOCUSED,
                      nullptr);
  lv_obj_add_event_cb(noteTitleArea_, noteTitleReadyEvent, LV_EVENT_READY,
                      nullptr);

  noteEditorOpen_ = true;
  noteDirty_ = false;
  noteKeyboardVisible_ = true;
  fullscreenApplicationActive_ = true;
  setTaskText(tr("Notes", "Заметки"));
  if (!path) {
    lv_obj_add_state(noteTitleArea_, LV_STATE_FOCUSED);
    lv_textarea_set_cursor_pos(noteTitleArea_, LV_TEXTAREA_CURSOR_LAST);
  } else {
    hideNoteKeyboard();
  }
}

void DesktopShell::openAbout() {
  lv_obj_t* content = createWindow(tr("About OSEsp32", "О системе OSEsp32"));
  lv_obj_t* title = lv_label_create(content);
  lv_label_set_text(title, tr("OSEsp32 0.3\nStorage and files preview",
                              "OSEsp32 0.3\nХранилище и файлы"));
  lv_obj_set_pos(title, 18, 14);
  lv_obj_set_style_text_color(title, lv_color_hex(COLOR_TITLE), 0);

  lv_obj_t* text = lv_label_create(content);
  lv_label_set_text(text, tr(
      "Arduino-ESP32 + FreeRTOS + LVGL\nESP32-2432S028 / ILI9341 / XPT2046\n\nExternal .yap applications: Stage 4",
      "Arduino-ESP32 + FreeRTOS + LVGL\nESP32-2432S028 / ILI9341 / XPT2046\n\nВнешние приложения .yap: этап 4"));
  lv_obj_set_pos(text, 18, 58);
}

void DesktopShell::updateClock() {
  if (!clockLabel_) return;
  const uint32_t seconds = millis() / 1000;
  if (seconds == lastClockSecond_) return;
  lastClockSecond_ = seconds;
  char clock[12];
  dateTime_.formatTime(clock, sizeof(clock));
  lv_label_set_text(clockLabel_, clock);
}

void DesktopShell::setTaskText(const char* text) {
  if (taskLabel_) lv_label_set_text(taskLabel_, text);
}

void DesktopShell::applyWallpaper() {
  removeWallpaper();
  if (!storage_.mounted() ||
      !settings_.loadWallpaper(wallpaperPath_, sizeof(wallpaperPath_)) ||
      !storage_.exists(wallpaperPath_)) {
    return;
  }

  if (strcmp(wallpaperPath_, WallpaperService::OPTIMIZED_SD_PATH)) {
    if (!StorageService::makeLvglPath(wallpaperPath_, wallpaperLvPath_,
                                      sizeof(wallpaperLvPath_)) ||
        !wallpaperService_.optimize(wallpaperLvPath_, 0x0B4F) ||
        !settings_.saveWallpaper(WallpaperService::OPTIMIZED_SD_PATH))
      return;
    strlcpy(wallpaperPath_, WallpaperService::OPTIMIZED_SD_PATH,
            sizeof(wallpaperPath_));
  }
  strlcpy(wallpaperLvPath_, WallpaperService::OPTIMIZED_LVGL_PATH,
          sizeof(wallpaperLvPath_));

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

void DesktopShell::applyDesktopColor() {
  if (!screen_) return;
  const DesktopColorOption& option = DESKTOP_COLORS[desktopColor_];
  lv_obj_set_style_bg_color(screen_, lv_color_hex(option.top), 0);
  lv_obj_set_style_bg_grad_color(screen_, lv_color_hex(option.bottom), 0);
  lv_obj_set_style_bg_grad_dir(screen_, LV_GRAD_DIR_VER, 0);
  lv_obj_invalidate(screen_);
}

void DesktopShell::showNoteKeyboard(lv_obj_t* textarea) {
  if (!noteEditorOpen_ || !noteKeyboard_ || !textarea) return;
  lv_obj_set_pos(noteKeyboard_, 3, 128);
  lv_obj_set_size(noteKeyboard_, 314, 109);
  lv_obj_remove_flag(noteKeyboard_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(noteHideKeyboardButton_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_height(noteBodyArea_, 56);
  lv_obj_move_foreground(noteKeyboard_);
  lv_keyboard_set_textarea(noteKeyboard_, textarea);
  lv_obj_invalidate(noteKeyboard_);
  noteKeyboardVisible_ = true;
}

void DesktopShell::hideNoteKeyboard() {
  if (!noteKeyboard_) return;
  lv_keyboard_set_textarea(noteKeyboard_, nullptr);
  if (noteTitleArea_) lv_obj_remove_state(noteTitleArea_, LV_STATE_FOCUSED);
  if (noteBodyArea_) lv_obj_remove_state(noteBodyArea_, LV_STATE_FOCUSED);
  lv_obj_add_flag(noteKeyboard_, LV_OBJ_FLAG_HIDDEN);
  if (noteHideKeyboardButton_)
    lv_obj_add_flag(noteHideKeyboardButton_, LV_OBJ_FLAG_HIDDEN);
  if (noteBodyArea_) lv_obj_set_height(noteBodyArea_, 166);
  noteKeyboardVisible_ = false;
}

bool DesktopShell::saveCurrentNote() {
  if (!noteEditorOpen_ || !storage_.mounted()) {
    showInfoDialog(tr("Insert the SD card before saving.",
                      "Вставьте SD-карту перед сохранением."));
    return false;
  }
  const char* title = lv_textarea_get_text(noteTitleArea_);
  const char* body = lv_textarea_get_text(noteBodyArea_);
  if (!title[0]) {
    title = tr("Untitled", "Без названия");
    lv_textarea_set_text(noteTitleArea_, title);
  }
  if (!notes_.save(notePath_, sizeof(notePath_), title, body)) {
    showInfoDialog(tr("Could not save the note.",
                      "Не удалось сохранить заметку."));
    return false;
  }
  noteDirty_ = false;
  setTaskText(tr("Note saved", "Заметка сохранена"));
  return true;
}

void DesktopShell::requestNoteExit() {
  if (!noteEditorOpen_) return;
  if (noteDirty_) {
    showNoteExitDialog();
    return;
  }
  closeWindow();
  openNotes();
}

void DesktopShell::showNoteExitDialog() {
  closeDialog();
  dialog_ = lv_obj_create(screen_);
  lv_obj_set_pos(dialog_, 20, 62);
  lv_obj_set_size(dialog_, 280, 114);
  configurePanel(dialog_, 0xF7F7F7, 4);
  lv_obj_set_style_border_width(dialog_, 2, 0);
  lv_obj_set_style_border_color(dialog_, lv_color_hex(COLOR_TITLE), 0);
  lv_obj_t* message = lv_label_create(dialog_);
  lv_label_set_text(message,
                    tr("Save changes before closing?",
                       "Сохранить изменения перед выходом?"));
  lv_obj_set_pos(message, 12, 14);
  lv_obj_set_width(message, 256);
  lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, 0);
  createButton(dialog_, tr("SAVE", "СОХР."), 7, 70, 80, 32,
               noteExitSaveEvent);
  createButton(dialog_, tr("DON'T SAVE", "НЕ СОХР."), 95, 70, 90, 32,
               noteExitDiscardEvent);
  createButton(dialog_, tr("CANCEL", "ОТМЕНА"), 193, 70, 80, 32,
               cancelDialogEvent);
  lv_obj_move_foreground(dialog_);
}

void DesktopShell::adjustPendingDateTime(uint8_t field, int8_t delta) {
  switch (field) {
    case 0:
      pendingDateTime_.tm_year = constrain(pendingDateTime_.tm_year + delta,
                                           120, 199);
      break;
    case 1:
      pendingDateTime_.tm_mon += delta;
      if (pendingDateTime_.tm_mon < 0) pendingDateTime_.tm_mon = 11;
      if (pendingDateTime_.tm_mon > 11) pendingDateTime_.tm_mon = 0;
      break;
    case 2:
      pendingDateTime_.tm_mday += delta;
      break;
    case 3:
      pendingDateTime_.tm_hour =
          (pendingDateTime_.tm_hour + delta + 24) % 24;
      break;
    case 4:
      pendingDateTime_.tm_min =
          (pendingDateTime_.tm_min + delta + 60) % 60;
      break;
    case 5:
      pendingTimezoneMinutes_ = constrain(
          pendingTimezoneMinutes_ + static_cast<int16_t>(delta) * 30,
          -720, 840);
      break;
  }
  const int maximumDay = daysInMonth(pendingDateTime_.tm_year + 1900,
                                     pendingDateTime_.tm_mon + 1);
  if (pendingDateTime_.tm_mday < 1) pendingDateTime_.tm_mday = maximumDay;
  if (pendingDateTime_.tm_mday > maximumDay) pendingDateTime_.tm_mday = 1;
}

void DesktopShell::updateScreenSaver() {
  if (screenSaverVisible_) {
    if (fullscreenApplicationActive_ || calibration_.active()) {
      hideScreenSaver();
      return;
    }
    if (screenSaverMode_ == ScreenSaverMode::Starfield)
      updateScreenSaverStars();
    else
      updateScreenSaverClock();
    return;
  }
  if (!screenSaverEnabled_ || fullscreenApplicationActive_ ||
      calibration_.active())
    return;
  if (lv_display_get_inactive_time(nullptr) >=
      SCREEN_SAVER_TIMEOUTS[screenSaverTimeoutIndex_])
    showScreenSaver();
}

void DesktopShell::showScreenSaver() {
  if (screenSaverVisible_ || fullscreenApplicationActive_ ||
      calibration_.active())
    return;
  screenSaverOverlay_ = lv_obj_create(screen_);
  lv_obj_set_pos(screenSaverOverlay_, 0, 0);
  lv_obj_set_size(screenSaverOverlay_, board::SCREEN_WIDTH,
                  board::SCREEN_HEIGHT);
  configurePanel(screenSaverOverlay_, 0x07131D);
  if (screenSaverMode_ == ScreenSaverMode::Clock) {
    lv_obj_set_style_bg_grad_color(screenSaverOverlay_, lv_color_hex(0x13293D),
                                   0);
    lv_obj_set_style_bg_grad_dir(screenSaverOverlay_, LV_GRAD_DIR_VER, 0);
  }
  lv_obj_add_flag(screenSaverOverlay_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(screenSaverOverlay_, screenSaverWakeEvent,
                      LV_EVENT_PRESSED, nullptr);

  screenSaverImageLvPath_[0] = '\0';
  if (screenSaverMode_ == ScreenSaverMode::Picture &&
      screenSaverImagePath_[0] && storage_.mounted() &&
      storage_.exists(screenSaverImagePath_) &&
      StorageService::makeLvglPath(screenSaverImagePath_,
                                   screenSaverImageLvPath_,
                                   sizeof(screenSaverImageLvPath_))) {
    lv_image_header_t header;
    if (lv_image_decoder_get_info(screenSaverImageLvPath_, &header) ==
        LV_RESULT_OK) {
      lv_obj_t* image = lv_image_create(screenSaverOverlay_);
      lv_obj_set_pos(image, 0, 0);
      lv_obj_set_size(image, board::SCREEN_WIDTH, board::SCREEN_HEIGHT);
      lv_image_set_inner_align(image, LV_IMAGE_ALIGN_CENTER);
      lv_image_set_src(image, screenSaverImageLvPath_);
      lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    } else {
      screenSaverImageLvPath_[0] = '\0';
    }
  }

  if (screenSaverMode_ == ScreenSaverMode::Clock) {
    lv_obj_t* clockPanel = lv_obj_create(screenSaverOverlay_);
    lv_obj_set_pos(clockPanel, 55, 69);
    lv_obj_set_size(clockPanel, 210, 102);
    configurePanel(clockPanel, 0x05090D, 10);
    lv_obj_remove_flag(clockPanel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(clockPanel, LV_OPA_70, 0);
    lv_obj_set_style_border_width(clockPanel, 1, 0);
    lv_obj_set_style_border_color(clockPanel, lv_color_hex(0xB7D7EA), 0);
    lv_obj_set_style_border_opa(clockPanel, LV_OPA_50, 0);

    screenSaverTimeLabel_ = lv_label_create(clockPanel);
    lv_obj_set_style_text_font(screenSaverTimeLabel_, &lv_font_montserrat_28,
                               0);
    lv_obj_set_style_text_color(screenSaverTimeLabel_, lv_color_white(), 0);
    lv_obj_set_width(screenSaverTimeLabel_, 210);
    lv_obj_set_pos(screenSaverTimeLabel_, 0, 15);
    lv_obj_set_style_text_align(screenSaverTimeLabel_, LV_TEXT_ALIGN_CENTER,
                                0);
    screenSaverDateLabel_ = lv_label_create(clockPanel);
    lv_obj_set_style_text_color(screenSaverDateLabel_,
                                lv_color_hex(0xD6E9F5), 0);
    lv_obj_set_width(screenSaverDateLabel_, 210);
    lv_obj_set_pos(screenSaverDateLabel_, 0, 59);
    lv_obj_set_style_text_align(screenSaverDateLabel_, LV_TEXT_ALIGN_CENTER,
                                0);
    updateScreenSaverClock();
  } else if (screenSaverMode_ == ScreenSaverMode::Starfield) {
    screenSaverStarField_ = lv_obj_create(screenSaverOverlay_);
    lv_obj_set_pos(screenSaverStarField_, 0, 0);
    lv_obj_set_size(screenSaverStarField_, board::SCREEN_WIDTH,
                    board::SCREEN_HEIGHT);
    configurePanel(screenSaverStarField_, 0x000000);
    lv_obj_remove_flag(screenSaverStarField_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screenSaverStarField_, screenSaverDrawStarsEvent,
                        LV_EVENT_DRAW_MAIN, nullptr);
    initializeScreenSaverStars();
  }
  lv_obj_move_foreground(screenSaverOverlay_);
  screenSaverVisible_ = true;
}

void DesktopShell::hideScreenSaver() {
  if (!screenSaverOverlay_) return;
  lv_obj_delete(screenSaverOverlay_);
  screenSaverOverlay_ = nullptr;
  screenSaverTimeLabel_ = nullptr;
  screenSaverDateLabel_ = nullptr;
  screenSaverStarField_ = nullptr;
  delete[] screenSaverStars_;
  screenSaverStars_ = nullptr;
  lastScreenSaverFrame_ = 0;
  screenSaverVisible_ = false;
  if (screenSaverImageLvPath_[0]) {
    lv_image_cache_drop(screenSaverImageLvPath_);
    screenSaverImageLvPath_[0] = '\0';
  }
  lv_display_trigger_activity(nullptr);
}

void DesktopShell::updateScreenSaverClock() {
  if (!screenSaverTimeLabel_ || !screenSaverDateLabel_) return;
  char time[12];
  char date[20];
  dateTime_.formatTime(time, sizeof(time));
  dateTime_.formatDate(date, sizeof(date));
  lv_label_set_text(screenSaverTimeLabel_, time);
  lv_label_set_text(screenSaverDateLabel_, date);
}

void DesktopShell::resetScreenSaverStar(ScreenSaverStar& star,
                                        bool randomDepth) {
  do {
    star.x = static_cast<int16_t>(random(-160, 161));
    star.y = static_cast<int16_t>(random(-120, 121));
  } while (abs(star.x) < 16 && abs(star.y) < 16);
  star.z = randomDepth ? static_cast<uint16_t>(random(48, 257)) : 256;
  star.previousZ = star.z;
}

void DesktopShell::initializeScreenSaverStars() {
  delete[] screenSaverStars_;
  screenSaverStars_ = new ScreenSaverStar[SCREEN_SAVER_STAR_COUNT];
  if (!screenSaverStars_) return;
  for (uint8_t index = 0; index < SCREEN_SAVER_STAR_COUNT; ++index)
    resetScreenSaverStar(screenSaverStars_[index], true);
  lastScreenSaverFrame_ = millis();
}

void DesktopShell::updateScreenSaverStars() {
  if (!screenSaverStarField_ || !screenSaverStars_) return;
  const uint32_t now = millis();
  if (now - lastScreenSaverFrame_ < 40) return;
  lastScreenSaverFrame_ = now;
  for (uint8_t index = 0; index < SCREEN_SAVER_STAR_COUNT; ++index) {
    ScreenSaverStar& star = screenSaverStars_[index];
    star.previousZ = star.z;
    star.z = star.z > 6 ? star.z - 6 : 1;
    const int32_t projectedX = 160 +
        static_cast<int32_t>(star.x) * 128 / star.z;
    const int32_t projectedY = 120 +
        static_cast<int32_t>(star.y) * 128 / star.z;
    if (star.z <= 6 || projectedX < -8 || projectedX > 327 ||
        projectedY < -8 || projectedY > 247)
      resetScreenSaverStar(star, false);
  }
  lv_obj_invalidate(screenSaverStarField_);
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
  lv_label_set_text(instruction, tr("Hold center, then release stylus",
                                    "Удерживайте центр, затем отпустите"));
  lv_obj_set_pos(instruction, 45, 112);
  lv_obj_set_style_text_color(instruction, lv_color_hex(0xB8C7D1), 0);

  calibrationProgress_ = lv_bar_create(calibrationOverlay_);
  lv_obj_set_pos(calibrationProgress_, 100, 138);
  lv_obj_set_size(calibrationProgress_, 120, 7);
  lv_bar_set_range(calibrationProgress_, 0, 100);

  lv_obj_t* cancel = createButton(calibrationOverlay_, tr("CANCEL", "ОТМЕНА"), 122, 3, 76,
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
  snprintf(title, sizeof(title), language_ == SystemLanguage::Russian
           ? "КАЛИБРОВКА %u / %u" : "TOUCH CALIBRATION %u / %u",
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
  setTaskText(success ? tr("Touch calibration saved", "Калибровка сохранена")
                      : tr("Calibration failed", "Ошибка калибровки"));
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
  setTaskText(tr("Touch calibration cancelled", "Калибровка отменена"));
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
  active_->setTaskText(active_->tr("Applying rotation...", "Применение поворота..."));
  delay(150);
  ESP.restart();
}

void DesktopShell::desktopColorSettingsEvent(lv_event_t*) {
  active_->openDesktopColorSettings();
}

void DesktopShell::desktopColorEvent(lv_event_t* event) {
  const uint8_t colorIndex = static_cast<uint8_t>(
      reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  if (colorIndex >= DESKTOP_COLOR_COUNT) return;
  if (!active_->settings_.saveDesktopColor(colorIndex)) {
    active_->showInfoDialog(active_->tr("Could not save desktop color.",
                                        "Не удалось сохранить цвет."));
    return;
  }
  active_->desktopColor_ = colorIndex;
  active_->applyDesktopColor();
  active_->openDesktopColorSettings();
  active_->setTaskText(active_->tr("Desktop color saved",
                                   "Цвет рабочего стола сохранён"));
}

void DesktopShell::settingsDisplayEvent(lv_event_t*) {
  active_->openDisplaySettings();
}

void DesktopShell::settingsLanguageEvent(lv_event_t*) {
  active_->openLanguageSettings();
}

void DesktopShell::settingsTouchEvent(lv_event_t*) {
  active_->openTouchSettings();
}

void DesktopShell::settingsDateTimeEvent(lv_event_t*) {
  active_->dateTimeScrollY_ = 0;
  active_->prepareDateTimeEdit();
  active_->openDateTimeSettings();
}

void DesktopShell::settingsScreenSaverEvent(lv_event_t*) {
  active_->openScreenSaverSettings();
}

void DesktopShell::settingsBackEvent(lv_event_t*) {
  active_->dateTimeEditPrepared_ = false;
  active_->dateTimeScrollY_ = 0;
  active_->openSettings();
}

void DesktopShell::languageEnglishEvent(lv_event_t*) {
  if (active_->language_ == SystemLanguage::English) return;
  if (!active_->settings_.saveLanguage(SystemLanguage::English)) {
    active_->showInfoDialog(active_->tr("Could not save language.",
                                        "Не удалось сохранить язык."));
    return;
  }
  active_->setTaskText(active_->tr("Restarting...", "Перезапуск..."));
  delay(150);
  ESP.restart();
}

void DesktopShell::languageRussianEvent(lv_event_t*) {
  if (active_->language_ == SystemLanguage::Russian) return;
  if (!active_->settings_.saveLanguage(SystemLanguage::Russian)) {
    active_->showInfoDialog(active_->tr("Could not save language.",
                                        "Не удалось сохранить язык."));
    return;
  }
  active_->setTaskText(active_->tr("Restarting...", "Перезапуск..."));
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
    active_->setTaskText(active_->tr("Could not request diagnostics",
                                     "Не удалось запустить диагностику"));
    active_->kernel_->faults().report(FaultCode::StorageUnavailable, "boot",
                                      "diagnostic request could not be saved");
    return;
  }
  active_->kernel_->logger().info("shell", "restarting into diagnostics");
  active_->setTaskText(active_->tr("Restarting...", "Перезапуск..."));
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
    active_->showInfoDialog(active_->tr(
        "No application is associated\nwith this file type yet.",
        "Для этого типа файлов\nпока нет приложения."));
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
  active_->setTaskText(active_->tr("Preparing wallpaper...", "Подготовка обоев..."));
  lv_refr_now(nullptr);
  if (!active_->wallpaperService_.optimize(active_->selectedImageLvPath_,
                                           0x0B4F) ||
      !active_->settings_.saveWallpaper(
          WallpaperService::OPTIMIZED_SD_PATH)) {
    active_->kernel_->faults().report(FaultCode::StorageUnavailable,
                                      "settings",
                                      "wallpaper could not be optimized");
    active_->showInfoDialog(active_->tr("Could not prepare this wallpaper.",
                                        "Не удалось подготовить обои."));
    return;
  }
  active_->applyWallpaper();
  active_->setTaskText(active_->tr("Wallpaper saved", "Обои установлены"));
}

void DesktopShell::clearWallpaperEvent(lv_event_t*) {
  active_->settings_.clearWallpaper();
  active_->removeWallpaper();
  active_->wallpaperService_.clearOptimizedFile();
  active_->setTaskText(active_->tr("Wallpaper cleared", "Обои удалены"));
}

void DesktopShell::setScreenSaverImageEvent(lv_event_t*) {
  if (!active_->settings_.saveScreenSaverImage(active_->selectedImagePath_)) {
    active_->showInfoDialog(active_->tr(
        "Could not save the screen saver image.",
        "Не удалось сохранить картинку заставки."));
    return;
  }
  strlcpy(active_->screenSaverImagePath_, active_->selectedImagePath_,
          sizeof(active_->screenSaverImagePath_));
  active_->screenSaverMode_ = ScreenSaverMode::Picture;
  active_->settings_.saveScreenSaverMode(
      static_cast<uint8_t>(active_->screenSaverMode_));
  active_->setTaskText(active_->tr("Screen saver image saved",
                                   "Картинка заставки сохранена"));
}

void DesktopShell::dateTimeAdjustEvent(lv_event_t* event) {
  if (active_->dateTimeContent_)
    active_->dateTimeScrollY_ =
        lv_obj_get_scroll_y(active_->dateTimeContent_);
  const uintptr_t encoded =
      reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
  active_->adjustPendingDateTime(static_cast<uint8_t>(encoded / 2),
                                 encoded % 2 ? 1 : -1);
  active_->openDateTimeSettings();
}

void DesktopShell::dateTimeSaveEvent(lv_event_t*) {
  if (!active_->dateTime_.setLocal(active_->pendingDateTime_,
                                   active_->pendingTimezoneMinutes_)) {
    active_->showInfoDialog(active_->tr("Could not save date and time.",
                                        "Не удалось сохранить дату и время."));
    return;
  }
  active_->dateTimeEditPrepared_ = false;
  active_->dateTimeScrollY_ = 0;
  active_->lastClockSecond_ = UINT32_MAX;
  active_->updateClock();
  active_->setTaskText(active_->tr("Date and time saved",
                                   "Дата и время сохранены"));
  active_->openSettings();
}

void DesktopShell::screenSaverEnabledEvent(lv_event_t*) {
  const bool enabled = !active_->screenSaverEnabled_;
  if (!active_->settings_.saveScreenSaverEnabled(enabled)) return;
  active_->screenSaverEnabled_ = enabled;
  if (!enabled) active_->hideScreenSaver();
  lv_display_trigger_activity(nullptr);
  active_->openScreenSaverSettings();
}

void DesktopShell::screenSaverTimeoutEvent(lv_event_t*) {
  active_->screenSaverTimeoutIndex_ =
      (active_->screenSaverTimeoutIndex_ + 1) % SCREEN_SAVER_TIMEOUT_COUNT;
  if (!active_->settings_.saveScreenSaverTimeout(
          active_->screenSaverTimeoutIndex_))
    return;
  lv_display_trigger_activity(nullptr);
  active_->openScreenSaverSettings();
}

void DesktopShell::screenSaverModeEvent(lv_event_t* event) {
  const bool next = reinterpret_cast<uintptr_t>(
                        lv_event_get_user_data(event)) != 0;
  int8_t mode = static_cast<int8_t>(active_->screenSaverMode_);
  mode += next ? 1 : -1;
  if (mode < 0) mode = static_cast<int8_t>(ScreenSaverMode::Starfield);
  if (mode > static_cast<int8_t>(ScreenSaverMode::Starfield)) mode = 0;
  const ScreenSaverMode selected = static_cast<ScreenSaverMode>(mode);
  if (!active_->settings_.saveScreenSaverMode(static_cast<uint8_t>(selected)))
    return;
  active_->screenSaverMode_ = selected;
  lv_display_trigger_activity(nullptr);
  active_->openScreenSaverSettings();
}

void DesktopShell::screenSaverChooseImageEvent(lv_event_t*) {
  strlcpy(active_->currentPath_, "/", sizeof(active_->currentPath_));
  active_->filePage_ = 0;
  active_->openFiles();
}

void DesktopShell::screenSaverClearImageEvent(lv_event_t*) {
  if (!active_->settings_.clearScreenSaverImage()) return;
  active_->screenSaverImagePath_[0] = '\0';
  active_->openScreenSaverSettings();
}

void DesktopShell::screenSaverWakeEvent(lv_event_t*) {
  active_->hideScreenSaver();
}

void DesktopShell::screenSaverDrawStarsEvent(lv_event_t* event) {
  if (!active_ || !active_->screenSaverStarField_ ||
      !active_->screenSaverStars_)
    return;
  lv_layer_t* layer = lv_event_get_layer(event);
  if (!layer) return;
  lv_area_t coordinates;
  lv_obj_get_coords(active_->screenSaverStarField_, &coordinates);
  for (uint8_t index = 0; index < SCREEN_SAVER_STAR_COUNT; ++index) {
    const ScreenSaverStar& star = active_->screenSaverStars_[index];
    if (!star.z || !star.previousZ) continue;
    const int32_t currentX = coordinates.x1 + 160 +
        static_cast<int32_t>(star.x) * 128 / star.z;
    const int32_t currentY = coordinates.y1 + 120 +
        static_cast<int32_t>(star.y) * 128 / star.z;
    const int32_t previousX = coordinates.x1 + 160 +
        static_cast<int32_t>(star.x) * 128 / star.previousZ;
    const int32_t previousY = coordinates.y1 + 120 +
        static_cast<int32_t>(star.y) * 128 / star.previousZ;
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = lv_color_white();
    const int32_t brightness = constrain(300 - static_cast<int32_t>(star.z),
                                         70, 255);
    line.opa = static_cast<lv_opa_t>(brightness);
    line.width = star.z < 72 ? 2 : 1;
    line.round_start = 1;
    line.round_end = 1;
    line.p1.x = previousX;
    line.p1.y = previousY;
    line.p2.x = currentX;
    line.p2.y = currentY;
    lv_draw_line(layer, &line);
  }
}

void DesktopShell::noteCardEvent(lv_event_t* event) {
  const uint8_t index = static_cast<uint8_t>(
      reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  if (index == NOTE_CREATE_INDEX) {
    active_->openNoteEditor();
    return;
  }
  if (index < active_->noteCount_)
    active_->openNoteEditor(active_->noteSummaries_[index].path);
}

void DesktopShell::noteTextChangedEvent(lv_event_t*) {
  if (active_->noteEditorOpen_) active_->noteDirty_ = true;
}

void DesktopShell::noteTextFocusedEvent(lv_event_t* event) {
  active_->showNoteKeyboard(lv_event_get_target_obj(event));
}

void DesktopShell::noteTitleReadyEvent(lv_event_t*) {
  if (!active_->noteBodyArea_) return;
  active_->showNoteKeyboard(active_->noteBodyArea_);
  lv_obj_add_state(active_->noteBodyArea_, LV_STATE_FOCUSED);
  lv_textarea_set_cursor_pos(active_->noteBodyArea_, 0);
}

void DesktopShell::noteBackEvent(lv_event_t*) { active_->requestNoteExit(); }

void DesktopShell::noteSaveEvent(lv_event_t*) { active_->saveCurrentNote(); }

void DesktopShell::noteHideKeyboardEvent(lv_event_t*) {
  active_->hideNoteKeyboard();
}

void DesktopShell::noteExitSaveEvent(lv_event_t*) {
  if (!active_->saveCurrentNote()) return;
  active_->closeWindow();
  active_->openNotes();
}

void DesktopShell::noteExitDiscardEvent(lv_event_t*) {
  active_->noteDirty_ = false;
  active_->closeWindow();
  active_->openNotes();
}
