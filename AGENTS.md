# OSEsp32 development guide

Read these files before changing the project:

1. `docs/PROJECT_MAP.md` — actual file ownership, boot flow, persistent data,
   build commands and known limitations.
2. `docs/ARCHITECTURE.md` — current and target architectural boundaries.
3. `docs/ROADMAP.md` — current stage and gates before the YAP runtime.
4. The matching `docs/STAGE_*.md` file for the work being performed.

Development rules:

- Arduino IDE is the primary build workflow. Keep `OSEsp32.ino` minimal and
  put implementation under `src/`.
- Verify with `PYTHONPATH=/tmp/yellowos-platformio python3 -m platformio run
  --environment cyd_stage3` when that local PlatformIO wrapper is available.
- Only code running from the Arduino/UI loop may mutate LVGL objects.
- Board pin numbers belong only in `src/board/BoardConfig.h`.
- Shell and applications must use `StorageService`; direct Arduino `SD` access
  is limited to that service and the mutually exclusive recovery diagnostics.
- Preserve fixed memory bounds. Do not add a full-screen framebuffer or a
  general SD swap file.
- A system overlay (keyboard, dialog, application exit control) has exactly one
  owner. Applications request it through an interface and never create or
  delete the shared object directly.
- Do not claim a hardware acceptance check passed merely because compilation
  succeeded. Record unverified behavior as such.
- Update `docs/PROJECT_MAP.md`, `docs/ARCHITECTURE.md` and `docs/ROADMAP.md`
  whenever ownership, persistent formats or stage boundaries change.

Current priority: Stage 3.1 stabilization. The isolated system keyboard passed
its initial on-board visibility/input check and now serves Notes through a
textarea adapter. Validate Notes field switching, repeated show/hide, close,
rotation and memory stability as described in `docs/SYSTEM_KEYBOARD.md`; Stage
4 remains blocked until those checks pass.
