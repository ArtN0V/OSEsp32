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
- Verify with `PYTHONPATH=/tmp/osesp32-platformio python3 -m platformio run
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

Current priorities: finish the Stage 4 hardware acceptance matrix and continue
Stage 5 Work package 4 Paint with bounded BMP import/export plus document/SD
recovery on top of the implemented API 1.4 drawing/touch slice. Do not let new
SDK work waive an open hardware gate. API 1.3 remains diagnostic; Paint uses
the additive API 1.4 I4 drawing surface. The code-complete slice includes
AppStorageService, the bounded API 1.2 UI
model/YapUiHost, system document pickers and FileAssociationService. Read
docs/YAP_API.md before runtime changes. Lua receives queued integer IDs/events,
not callbacks or pointers; all file handles are session capabilities. Never recover transaction slots while a live session owns
them: first invalidate handles and pause, then recover on explicit Retry.
Run tools/test_yap_runtime.py after runtime/storage changes; it tests actual Lua,
package parser and AppStorageService with ASan/UBSan and mocked physical I/O.

Fullscreen/exclusive must not show a system EXIT button or title bar. Apps
call osesp32.exit() for normal return; keep the invisible two-second top-left
hold for recovery. Windowed retains EXIT. Explicit app exit skips the report.
