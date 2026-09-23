#pragma once

#include <stdint.h>

// UI callbacks only enqueue intent. The UI loop advances one phase per tick.
class AppLifecycle {
 public:
  enum class State : uint8_t { Idle, Preparing, Running, Stopping, RestoringShell };
  State state() const { return state_; }
  bool active() const { return state_ != State::Idle; }
  bool begin() {
    if (active()) return false;
    state_ = State::Preparing;
    exitRequested_ = false;
    return true;
  }
  void requestExit() { if (active()) exitRequested_ = true; }
  bool exitRequested() const { return exitRequested_; }
  void prepared(bool success) {
    if (state_ == State::Preparing)
      state_ = success && !exitRequested_ ? State::Running : State::Stopping;
  }
  void stop() { if (active()) state_ = State::Stopping; }
  void stopped() {
    if (state_ == State::Stopping) state_ = State::RestoringShell;
  }
  void restored() {
    if (state_ == State::RestoringShell) {
      state_ = State::Idle;
      exitRequested_ = false;
    }
  }
 private:
  State state_ = State::Idle;
  bool exitRequested_ = false;
};
