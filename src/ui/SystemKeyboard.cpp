#include "SystemKeyboard.h"

#include <esp_heap_caps.h>
#include <string.h>

#include "../board/BoardConfig.h"

namespace {
constexpr uint32_t COLOR_PANEL = 0xD7DCE1;
constexpr uint32_t COLOR_KEY = 0xF8F9FA;
constexpr uint32_t COLOR_KEY_PRESSED = 0xC8E4FA;
constexpr uint32_t COLOR_CONTROL = 0x0078D4;
constexpr uint32_t COLOR_BORDER = 0xAAB2BA;

const char* const ENGLISH_LOWER[] = {
    "1#", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p",
    LV_SYMBOL_BACKSPACE, "\n",
    "ABC", "a", "s", "d", "f", "g", "h", "j", "k", "l",
    LV_SYMBOL_NEW_LINE, "\n",
    "_", "-", "z", "x", "c", "v", "b", "n", "m", ".", ",", ":",
    "\n",
    LV_SYMBOL_KEYBOARD, LV_SYMBOL_LEFT, "English", LV_SYMBOL_RIGHT, LV_SYMBOL_OK,
    ""};

const char* const ENGLISH_UPPER[] = {
    "1#", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P",
    LV_SYMBOL_BACKSPACE, "\n",
    "abc", "A", "S", "D", "F", "G", "H", "J", "K", "L",
    LV_SYMBOL_NEW_LINE, "\n",
    "_", "-", "Z", "X", "C", "V", "B", "N", "M", ".", ",", ":",
    "\n",
    LV_SYMBOL_KEYBOARD, LV_SYMBOL_LEFT, "English", LV_SYMBOL_RIGHT, LV_SYMBOL_OK,
    ""};

const char* const RUSSIAN_LOWER[] = {
    "1#", "й", "ц", "у", "к", "е", "н", "г", "ш", "щ", "з", "х",
    "ъ", LV_SYMBOL_BACKSPACE, "\n",
    "ABC", "ф", "ы", "в", "а", "п", "р", "о", "л", "д", "ж", "э",
    LV_SYMBOL_NEW_LINE, "\n",
    "ё", "я", "ч", "с", "м", "и", "т", "ь", "б", "ю", ".", ",",
    "\n",
    LV_SYMBOL_KEYBOARD, LV_SYMBOL_LEFT, "Русский", LV_SYMBOL_RIGHT, LV_SYMBOL_OK,
    ""};

const char* const RUSSIAN_UPPER[] = {
    "1#", "Й", "Ц", "У", "К", "Е", "Н", "Г", "Ш", "Щ", "З", "Х",
    "Ъ", LV_SYMBOL_BACKSPACE, "\n",
    "abc", "Ф", "Ы", "В", "А", "П", "Р", "О", "Л", "Д", "Ж", "Э",
    LV_SYMBOL_NEW_LINE, "\n",
    "Ё", "Я", "Ч", "С", "М", "И", "Т", "Ь", "Б", "Ю", ".", ",",
    "\n",
    LV_SYMBOL_KEYBOARD, LV_SYMBOL_LEFT, "Русский", LV_SYMBOL_RIGHT, LV_SYMBOL_OK,
    ""};

const char* const SYMBOLS_ENGLISH[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
    LV_SYMBOL_BACKSPACE, "\n",
    "abc", "+", "&", "/", "*", "=", "%", "!", "?", "#", "<", ">",
    "\n",
    "\\", "@", "$", "(", ")", "{", "}", "[", "]", ";", "\"", "'",
    "\n",
    LV_SYMBOL_KEYBOARD, LV_SYMBOL_LEFT, "English", LV_SYMBOL_RIGHT, LV_SYMBOL_OK,
    ""};

const char* const SYMBOLS_RUSSIAN[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
    LV_SYMBOL_BACKSPACE, "\n",
    "абв", "+", "&", "/", "*", "=", "%", "!", "?", "#", "<", ">",
    "\n",
    "\\", "@", "$", "(", ")", "{", "}", "[", "]", ";", "\"", "'",
    "\n",
    LV_SYMBOL_KEYBOARD, LV_SYMBOL_LEFT, "Русский", LV_SYMBOL_RIGHT, LV_SYMBOL_OK,
    ""};

bool isControlKey(const char* text) {
  return !strcmp(text, "1#") || !strcmp(text, "ABC") ||
         !strcmp(text, "abc") || !strcmp(text, "абв") ||
         !strcmp(text, LV_SYMBOL_BACKSPACE) ||
         !strcmp(text, LV_SYMBOL_NEW_LINE) ||
         !strcmp(text, LV_SYMBOL_KEYBOARD) || !strcmp(text, LV_SYMBOL_LEFT) ||
         !strcmp(text, LV_SYMBOL_RIGHT) || !strcmp(text, LV_SYMBOL_OK);
}

bool isSpaceKey(const char* text) {
  return text && (!strcmp(text, "English") || !strcmp(text, "Русский"));
}
}  // namespace

bool LvglTextareaInputClient::targetValid() const {
  return textarea_ && lv_obj_is_valid(textarea_);
}

void LvglTextareaInputClient::insertUtf8(const char* text) {
  if (targetValid() && text) lv_textarea_add_text(textarea_, text);
}

void LvglTextareaInputClient::backspace() {
  if (targetValid()) lv_textarea_delete_char(textarea_);
}

void LvglTextareaInputClient::moveCursor(int8_t direction) {
  if (!targetValid()) return;
  if (direction < 0)
    lv_textarea_cursor_left(textarea_);
  else if (direction > 0)
    lv_textarea_cursor_right(textarea_);
}

void LvglTextareaInputClient::enter() {
  if (!targetValid()) return;
  if (lv_textarea_get_one_line(textarea_))
    lv_obj_send_event(textarea_, LV_EVENT_READY, nullptr);
  else
    lv_textarea_add_char(textarea_, '\n');
}

void LvglTextareaInputClient::done() {
  if (targetValid()) lv_obj_send_event(textarea_, LV_EVENT_READY, nullptr);
}

void LvglTextareaInputClient::keyboardVisibilityChanged(
    bool visible, uint16_t coveredHeight) {
  if (visibilityCallback_)
    visibilityCallback_(visible, coveredHeight, visibilityUserData_);
}

bool SystemKeyboard::begin(lv_obj_t* overlayParent, const lv_font_t* font,
                           Logger& logger) {
  shutdown();
  if (!overlayParent || !lv_obj_is_valid(overlayParent)) return false;
  overlayParent_ = overlayParent;
  font_ = font;
  logger_ = &logger;
  state_ = KeyboardState::Hidden;
  metrics_ = {};
  metrics_.state = state_;
  return true;
}

bool SystemKeyboard::createObjects() {
  if (root_) return true;
  if (!overlayParent_ || !lv_obj_is_valid(overlayParent_)) return false;
  root_ = lv_obj_create(overlayParent_);
  if (!root_) return false;
  lv_obj_set_pos(root_, 0, board::SCREEN_HEIGHT - HEIGHT);
  lv_obj_set_size(root_, board::SCREEN_WIDTH, HEIGHT);
  lv_obj_set_style_pad_all(root_, 2, 0);
  lv_obj_set_style_radius(root_, 0, 0);
  lv_obj_set_style_border_width(root_, 1, 0);
  lv_obj_set_style_border_color(root_, lv_color_hex(COLOR_BORDER), 0);
  lv_obj_set_style_bg_color(root_, lv_color_hex(COLOR_PANEL), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

  matrix_ = lv_buttonmatrix_create(root_);
  if (!matrix_) {
    lv_obj_delete(root_);
    root_ = nullptr;
    return false;
  }
  lv_obj_set_pos(matrix_, 0, 0);
  lv_obj_set_size(matrix_, board::SCREEN_WIDTH - 6, HEIGHT - 6);
  lv_obj_set_style_pad_all(matrix_, 1, 0);
  lv_obj_set_style_pad_row(matrix_, 1, 0);
  lv_obj_set_style_pad_column(matrix_, 1, 0);
  lv_obj_set_style_bg_color(matrix_, lv_color_hex(COLOR_PANEL), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(matrix_, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(matrix_, lv_color_hex(COLOR_KEY), LV_PART_ITEMS);
  lv_obj_set_style_bg_color(matrix_, lv_color_hex(COLOR_KEY_PRESSED),
                            LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(matrix_, lv_color_hex(COLOR_CONTROL),
                            LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(matrix_, lv_color_hex(0x202020), LV_PART_ITEMS);
  lv_obj_set_style_text_color(matrix_, lv_color_white(),
                              LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_width(matrix_, 1, LV_PART_ITEMS);
  lv_obj_set_style_border_color(matrix_, lv_color_hex(COLOR_BORDER),
                                LV_PART_ITEMS);
  lv_obj_set_style_radius(matrix_, 2, LV_PART_ITEMS);
  if (font_) lv_obj_set_style_text_font(matrix_, font_, LV_PART_ITEMS);
  lv_obj_add_event_cb(matrix_, matrixEvent, LV_EVENT_ALL, this);
  return true;
}

uint16_t SystemKeyboard::countButtons(const char* const* map) {
  uint16_t count = 0;
  if (!map) return count;
  for (uint16_t index = 0; map[index][0]; ++index)
    if (strcmp(map[index], "\n")) ++count;
  return count;
}

void SystemKeyboard::applyLayout(KeyboardLanguage language,
                                 KeyboardLayout layout) {
  language_ = language;
  layout_ = layout;
  const char* const* map = ENGLISH_LOWER;
  if (layout == KeyboardLayout::Symbols)
    map = language == KeyboardLanguage::Russian ? SYMBOLS_RUSSIAN
                                                : SYMBOLS_ENGLISH;
  else if (language == KeyboardLanguage::Russian)
    map = layout == KeyboardLayout::Upper ? RUSSIAN_UPPER : RUSSIAN_LOWER;
  else
    map = layout == KeyboardLayout::Upper ? ENGLISH_UPPER : ENGLISH_LOWER;
  lv_buttonmatrix_set_map(matrix_, map);
  metrics_.keyCount = countButtons(map);
  configureButtonControls();
}

void SystemKeyboard::configureButtonControls() {
  for (uint16_t index = 0; index < metrics_.keyCount; ++index) {
    const char* text = lv_buttonmatrix_get_button_text(matrix_, index);
    if (!text) continue;
    uint8_t width = 2;
    if (!strcmp(text, "1#") || !strcmp(text, "ABC") ||
        !strcmp(text, "abc") || !strcmp(text, "абв") ||
        !strcmp(text, LV_SYMBOL_KEYBOARD) || !strcmp(text, LV_SYMBOL_OK))
      width = 4;
    else if (!strcmp(text, LV_SYMBOL_BACKSPACE) ||
             !strcmp(text, LV_SYMBOL_NEW_LINE))
      width = 5;
    else if (!strcmp(text, LV_SYMBOL_LEFT) ||
             !strcmp(text, LV_SYMBOL_RIGHT))
      width = 3;
    else if (isSpaceKey(text))
      width = 10;
    lv_buttonmatrix_set_button_width(matrix_, index, width);
    if (isSpaceKey(text))
      lv_buttonmatrix_set_button_ctrl(
          matrix_, index, static_cast<lv_buttonmatrix_ctrl_t>(
                              LV_BUTTONMATRIX_CTRL_CLICK_TRIG |
                              LV_BUTTONMATRIX_CTRL_NO_REPEAT));
    else if (isControlKey(text))
      lv_buttonmatrix_set_button_ctrl(matrix_, index,
                                      LV_BUTTONMATRIX_CTRL_CHECKED);
  }
}

bool SystemKeyboard::show(TextInputClient& client, KeyboardLanguage language,
                          KeyboardLayout layout) {
  if (state_ == KeyboardState::Detached) return false;
  metrics_.heapBeforeShow = ESP.getFreeHeap();
  if (!createObjects()) {
    if (logger_) logger_->error("keyboard", "could not create button matrix");
    return false;
  }
  if (client_ && client_ != &client)
    client_->keyboardVisibilityChanged(false, 0);
  client_ = &client;
  applyLayout(language, layout);
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(root_);
  lv_obj_update_layout(root_);
  state_ = KeyboardState::Visible;
  ++metrics_.showCount;
  refreshMetrics();
  if (metrics_.width != board::SCREEN_WIDTH || metrics_.height != HEIGHT) {
    if (logger_)
      logger_->error("keyboard", "invalid geometry %d,%d %dx%d", metrics_.x,
                     metrics_.y, metrics_.width, metrics_.height);
    hide();
    return false;
  }
  client_->keyboardVisibilityChanged(true, HEIGHT);
  lv_obj_invalidate(root_);
  if (logger_)
    logger_->info("keyboard", "visible lang=%u layout=%u keys=%u heap=%u",
                  static_cast<unsigned>(language_),
                  static_cast<unsigned>(layout_), metrics_.keyCount,
                  metrics_.heapAfterShow);
  return true;
}

void SystemKeyboard::hide() {
  if (state_ == KeyboardState::Detached) return;
  TextInputClient* previousClient = client_;
  client_ = nullptr;
  if (root_) lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
  if (state_ == KeyboardState::Visible) ++metrics_.hideCount;
  state_ = KeyboardState::Hidden;
  refreshMetrics();
  if (previousClient) previousClient->keyboardVisibilityChanged(false, 0);
  if (logger_) logger_->info("keyboard", "hidden");
}

void SystemKeyboard::shutdown() {
  if (state_ != KeyboardState::Detached) hide();
  if (root_) lv_obj_delete(root_);
  root_ = nullptr;
  matrix_ = nullptr;
  client_ = nullptr;
  overlayParent_ = nullptr;
  font_ = nullptr;
  logger_ = nullptr;
  state_ = KeyboardState::Detached;
  metrics_.state = state_;
}

void SystemKeyboard::refreshMetrics() {
  metrics_.state = state_;
  metrics_.language = language_;
  metrics_.layout = layout_;
  metrics_.clientAttached = client_ != nullptr;
  metrics_.heapAfterShow = ESP.getFreeHeap();
  metrics_.largestBlockAfterShow =
      heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  if (root_ && lv_obj_is_valid(root_)) {
    metrics_.x = lv_obj_get_x(root_);
    metrics_.y = lv_obj_get_y(root_);
    metrics_.width = lv_obj_get_width(root_);
    metrics_.height = lv_obj_get_height(root_);
  } else {
    metrics_.x = metrics_.y = metrics_.width = metrics_.height = 0;
  }
}

void SystemKeyboard::handleKey(const char* text) {
  if (!client_ || !text) return;
  bool layoutChanged = false;
  if (!strcmp(text, "1#")) {
    applyLayout(language_, KeyboardLayout::Symbols);
    layoutChanged = true;
  } else if (!strcmp(text, "ABC")) {
    applyLayout(language_, KeyboardLayout::Upper);
    layoutChanged = true;
  } else if (!strcmp(text, "abc") || !strcmp(text, "абв")) {
    applyLayout(language_, KeyboardLayout::Lower);
    layoutChanged = true;
  } else if (!strcmp(text, LV_SYMBOL_BACKSPACE)) {
    client_->backspace();
  } else if (!strcmp(text, LV_SYMBOL_NEW_LINE)) {
    client_->enter();
  } else if (!strcmp(text, LV_SYMBOL_LEFT)) {
    client_->moveCursor(-1);
  } else if (!strcmp(text, LV_SYMBOL_RIGHT)) {
    client_->moveCursor(1);
  } else if (!strcmp(text, LV_SYMBOL_KEYBOARD)) {
    hide();
  } else if (!strcmp(text, LV_SYMBOL_OK)) {
    client_->done();
  } else if (isSpaceKey(text)) {
    client_->insertUtf8(" ");
  } else {
    client_->insertUtf8(text);
  }
  refreshMetrics();
  if (layoutChanged && client_)
    client_->keyboardVisibilityChanged(true, HEIGHT);
}

void SystemKeyboard::switchLanguage(int8_t direction) {
  language_ = language_ == KeyboardLanguage::English
                  ? KeyboardLanguage::Russian
                  : KeyboardLanguage::English;
  applyLayout(language_, layout_);
  refreshMetrics();
  if (client_) client_->keyboardVisibilityChanged(true, HEIGHT);
  if (logger_)
    logger_->info("keyboard", "space swipe %s; language=%u",
                  direction < 0 ? "left" : "right",
                  static_cast<unsigned>(language_));
}

void SystemKeyboard::handlePointerEvent(lv_event_t* event) {
  const lv_event_code_t code = lv_event_get_code(event);
  lv_indev_t* input = lv_event_get_indev(event);
  if (code == LV_EVENT_PRESSED) {
    spaceGestureTracking_ = false;
    spaceGestureHandled_ = false;
    const uint32_t selected = lv_buttonmatrix_get_selected_button(matrix_);
    if (selected == LV_BUTTONMATRIX_BUTTON_NONE || !input) return;
    const char* text = lv_buttonmatrix_get_button_text(matrix_, selected);
    if (!isSpaceKey(text)) return;
    lv_indev_get_point(input, &spaceGestureStart_);
    spaceGestureStartedMs_ = millis();
    spaceGestureTracking_ = true;
    return;
  }
  if (code == LV_EVENT_PRESSING && spaceGestureTracking_ &&
      !spaceGestureHandled_ && input) {
    lv_point_t point;
    lv_indev_get_point(input, &point);
    const int32_t distance = point.x - spaceGestureStart_.x;
    if (millis() - spaceGestureStartedMs_ >= 300 &&
        (distance <= -32 || distance >= 32)) {
      spaceGestureHandled_ = true;
      switchLanguage(distance < 0 ? -1 : 1);
    }
    return;
  }
  if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    spaceGestureTracking_ = false;
    spaceGestureHandled_ = false;
  }
}

void SystemKeyboard::matrixEvent(lv_event_t* event) {
  SystemKeyboard* keyboard = static_cast<SystemKeyboard*>(
      lv_event_get_user_data(event));
  lv_obj_t* matrix = lv_event_get_target_obj(event);
  if (!keyboard || !matrix) return;
  const lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING ||
      code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    keyboard->handlePointerEvent(event);
    return;
  }
  if (code != LV_EVENT_VALUE_CHANGED) return;
  const uint32_t selected = lv_buttonmatrix_get_selected_button(matrix);
  if (selected == LV_BUTTONMATRIX_BUTTON_NONE) return;
  const char* text = lv_buttonmatrix_get_button_text(matrix, selected);
  if (isSpaceKey(text) && keyboard->spaceGestureHandled_) return;
  keyboard->handleKey(text);
}
