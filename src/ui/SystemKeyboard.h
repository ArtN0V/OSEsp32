#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "../kernel/Logger.h"

enum class KeyboardLanguage : uint8_t {
  English = 0,
  Russian = 1,
};

enum class KeyboardLayout : uint8_t {
  Lower = 0,
  Upper = 1,
  Symbols = 2,
};

enum class KeyboardState : uint8_t {
  Detached = 0,
  Hidden = 1,
  Visible = 2,
};

class TextInputClient {
 public:
  virtual ~TextInputClient() = default;
  virtual void insertUtf8(const char* text) = 0;
  virtual void backspace() = 0;
  virtual void moveCursor(int8_t direction) = 0;
  virtual void enter() = 0;
  virtual void done() = 0;
  virtual void keyboardVisibilityChanged(bool visible,
                                         uint16_t coveredHeight) = 0;
};

using KeyboardVisibilityCallback = void (*)(bool visible,
                                            uint16_t coveredHeight,
                                            void* userData);

class LvglTextareaInputClient final : public TextInputClient {
 public:
  void setTarget(lv_obj_t* textarea) { textarea_ = textarea; }
  void setVisibilityCallback(KeyboardVisibilityCallback callback,
                             void* userData) {
    visibilityCallback_ = callback;
    visibilityUserData_ = userData;
  }

  void insertUtf8(const char* text) override;
  void backspace() override;
  void moveCursor(int8_t direction) override;
  void enter() override;
  void done() override;
  void keyboardVisibilityChanged(bool visible,
                                 uint16_t coveredHeight) override;

 private:
  lv_obj_t* textarea_ = nullptr;
  KeyboardVisibilityCallback visibilityCallback_ = nullptr;
  void* visibilityUserData_ = nullptr;

  bool targetValid() const;
};

struct SystemKeyboardMetrics {
  KeyboardState state = KeyboardState::Detached;
  KeyboardLanguage language = KeyboardLanguage::English;
  KeyboardLayout layout = KeyboardLayout::Lower;
  uint16_t keyCount = 0;
  int16_t x = 0;
  int16_t y = 0;
  int16_t width = 0;
  int16_t height = 0;
  bool clientAttached = false;
  uint32_t showCount = 0;
  uint32_t hideCount = 0;
  uint32_t heapBeforeShow = 0;
  uint32_t heapAfterShow = 0;
  uint32_t largestBlockAfterShow = 0;
};

class SystemKeyboard {
 public:
  static constexpr uint16_t HEIGHT = 112;

  bool begin(lv_obj_t* overlayParent, const lv_font_t* font, Logger& logger);
  bool show(TextInputClient& client, KeyboardLanguage language,
            KeyboardLayout layout = KeyboardLayout::Lower);
  void hide();
  void shutdown();
  bool visible() const { return state_ == KeyboardState::Visible; }
  KeyboardState state() const { return state_; }
  const SystemKeyboardMetrics& metrics() const { return metrics_; }

 private:
  lv_obj_t* overlayParent_ = nullptr;
  lv_obj_t* root_ = nullptr;
  lv_obj_t* matrix_ = nullptr;
  const lv_font_t* font_ = nullptr;
  Logger* logger_ = nullptr;
  TextInputClient* client_ = nullptr;
  KeyboardLanguage language_ = KeyboardLanguage::English;
  KeyboardLayout layout_ = KeyboardLayout::Lower;
  KeyboardState state_ = KeyboardState::Detached;
  SystemKeyboardMetrics metrics_;
  bool spaceGestureTracking_ = false;
  bool spaceGestureHandled_ = false;
  lv_point_t spaceGestureStart_ = {};
  uint32_t spaceGestureStartedMs_ = 0;

  bool createObjects();
  void applyLayout(KeyboardLanguage language, KeyboardLayout layout);
  void configureButtonControls();
  void refreshMetrics();
  void handleKey(const char* text);
  void handlePointerEvent(lv_event_t* event);
  void switchLanguage(int8_t direction);
  static uint16_t countButtons(const char* const* map);
  static void matrixEvent(lv_event_t* event);
};
