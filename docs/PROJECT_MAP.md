# OSEsp32 project map

This document describes the code that exists now. Planned components are
called out explicitly and must not be mistaken for implemented code.

## Current status

- Target: ESP32-2432S028 without PSRAM, ILI9341 320×240, XPT2046 touch.
- Primary tool: Arduino IDE; `OSEsp32.ino` is the entry point.
- Reproducible check: PlatformIO environment `cyd_stage3` with LVGL 9.5.0.
- Current roadmap stage: **3.1 stabilization and system UI extraction**.
- The custom system keyboard passed its initial on-board visibility and input
  check and now serves Notes through an adapter. Repetition, close, rotation and
  memory-stability checks in `SYSTEM_KEYBOARD.md` remain open.

## Boot and update flow

```text
OSEsp32.ino / src/main.cpp
  -> OSEsp32App::begin()
     -> SystemKernel::begin()
     -> BootModeService::consumeRequestedMode()
     -> DiagnosticsApp::begin() OR DesktopShell::begin()

loop()
  -> OSEsp32App::update()
     -> SystemKernel::update()
     -> active DiagnosticsApp::update() OR DesktopShell::update()
```

Diagnostics and the graphical shell never run at the same time. This is why
the diagnostic `SdCardDriver` and shell `StorageService` can currently use the
Arduino global `SD` implementation without concurrent access.

## Source ownership

| Path | Actual responsibility | Important constraints |
|---|---|---|
| `OSEsp32.ino` | Arduino entry point | Keep implementation out of the sketch. |
| `src/main.cpp` | PlatformIO-only entry point | Guarded by `PLATFORMIO`; must not create duplicate Arduino symbols. |
| `src/app/OSEsp32App.*` | Chooses shell or diagnostics | Future composition root for shared graphical services. |
| `src/board/BoardConfig.h` | All CYD pins and physical constants | No GPIO literals elsewhere. |
| `src/board/DisplayDriver.*` | LovyanGFX ILI9341 bus/panel/backlight | HSPI, DMA channel 1, landscape rotation. |
| `src/board/TouchDriver.*` | Software-SPI XPT2046 read and coordinate mapping | Own NVS namespace for calibration; no XPT2046 library. |
| `src/board/SdCardDriver.*` | Recovery diagnostic SD tests | Only valid while diagnostics owns the foreground mode. |
| `src/kernel/*` | Static event queue, log ring, faults, memory monitoring and lifecycle | No UI objects or hardware pin knowledge. |
| `src/services/BootModeService.*` | One-shot diagnostics boot request | NVS-backed. |
| `src/services/SystemSettingsService.*` | Shell settings persistence | NVS-backed; currently one method per key. |
| `src/services/DateTimeService.*` | Software UTC clock and local offset | No RTC and no Wi-Fi source yet. |
| `src/services/StorageService.*` | Shell SD owner and LVGL `S:` bridge | All public paths are canonicalized; replacement preserves a backup. |
| `src/services/WallpaperService.*` | OWP1 conversion and two-strip decoder cache | Uses private LVGL decoder APIs pinned to LVGL 9.5.0. |
| `src/services/NotesService.*` | Bounded `.note` listing/load/save | Text documents only; UI belongs elsewhere. |
| `src/services/TouchCalibrationService.*` | Five-point raw-axis fit | Shared algorithm; graphical overlay is still in `DesktopShell`. |
| `src/services/LocalizationService.h` | English/Russian selector helper | String catalog is currently distributed through shell call sites. |
| `src/ui/LvglPort.*` | LVGL display, partial buffers and pointer adapter | The only current LVGL port; called cooperatively from the Arduino loop. |
| `src/ui/SystemKeyboard.*` | One OS-owned button-matrix keyboard and application-neutral input-client adapter | Lives on `lv_layer_top()`; lazily retains its hidden object tree and supports explicit `shutdown()`. Shared by Notes and Keyboard Test. |
| `src/ui/OSEsp32Font*` | Embedded regular/bold ASCII+Cyrillic fonts | Generated assets; see `THIRD_PARTY.md`. |
| `src/shell/DesktopShell.*` | Desktop plus every built-in graphical application | Current monolith and main refactoring target. |
| `src/diagnostics/DiagnosticsApp.*` | Recovery test UI and Serial commands | Intentionally synchronous and English-only. |

## Current graphical object ownership

`DesktopShell` owns the active screen, desktop, one foreground window, dialogs,
calibration overlay, Notes editor, screen saver and the first `SystemKeyboard`
instance. The system keyboard alone owns its overlay object tree;
`DesktopShell` currently acts as its composition root and hosts separate Notes
and test input adapters. LVGL callbacks route through the static
`DesktopShell::active_` pointer. This works for a single shell instance but is
not the final composition boundary for reusable system UI.

Target ownership is described in `SYSTEM_KEYBOARD.md`: keyboard and later file
pickers/dialogs become system overlays with one owner and request-based APIs.
Built-in applications must stop owning shared overlay objects.

## Memory model

- No PSRAM is assumed.
- LVGL renders with two `320×20` RGB565 partial buffers: about 25 KiB total.
- OWP wallpaper keeps two additional `320×20` RGB565 strips: about 25 KiB.
- Notes holds at most 24 summaries plus one bounded title/body edit buffer.
- Starfield state is allocated only while that saver is visible.
- Full-screen framebuffers and general virtual memory/swap are prohibited.
- Always watch both free heap and largest free block; total free bytes alone do
  not reveal fragmentation.

## Persistent data

### SD card

| Path | Owner | Format |
|---|---|---|
| `/OSEsp32/Apps` | future package manager | `.yap`, not implemented yet |
| `/OSEsp32/Data` | future application storage | per-app directories, Stage 4 |
| `/OSEsp32/Notes` | `NotesService` | first line title, remaining UTF-8 body, `.note` |
| `/OSEsp32/Wallpapers/desktop.owp` | `WallpaperService` | packed `OWP1`, 320×204 RGB565 |

Recognized user images are uncompressed BMP 16/24/32-bit and baseline JPEG.
Progressive JPEG, PNG and arbitrary scaling are not implemented.

### NVS namespaces

| Namespace | Owner | Contents |
|---|---|---|
| `osesp32_cfg` | `SystemSettingsService` | brightness, rotation, wallpaper, desktop color, language, clock, saver settings |
| `osesp32_touch` | `TouchDriver` | versioned calibration ranges and inversion flags |
| `yellow_touch` | `TouchDriver` | legacy calibration migration source |
| `osesp32_boot` | `BootModeService` | one-shot next boot mode |

`osesp32_cfg` currently uses keys `brightness`, `rotate180`, `wallpaper`,
`desk_color`, `language`, `clock_utc`, `clock_zone`, `ss_enabled`,
`ss_timeout`, `ss_mode` and `ss_image`. `osesp32_touch` uses `version`, `xmin`,
`xmax`, `ymin`, `ymax`, `invx` and `invy`; `osesp32_boot` uses `next`.

The software clock stores a checkpoint only. Powered-off time is unknowable
until an RTC or future network synchronization source is added.

## Where to make common changes

- Pins/controller timing: `src/board/BoardConfig.h` and matching driver.
- Desktop/window visuals and built-in apps: currently `DesktopShell.cpp`.
- Persistent setting: add paired load/save methods in
  `SystemSettingsService`, then document its NVS key.
- New SD-backed built-in data type: add a service over `StorageService`; do not
  use `SD` directly in the shell.
- Display/touch integration: `LvglPort`.
- Keyboard/input behavior: after Stage 3.1, only `SystemKeyboard`; applications
  submit requests and adapters.
- YAP design: `STAGE_4.md`; no runtime code exists yet.

## Verification commands

```text
PYTHONPATH=/tmp/yellowos-platformio python3 -m platformio run --environment cyd_stage3
git diff --check
```

Compilation proves API and memory-layout compatibility, not touch/display
behavior. Hardware changes require the acceptance checklist for their stage.

## Known debt and gates

1. `DesktopShell.cpp` is too large and mixes window management with application
   logic. Extract system overlays first, then built-in applications gradually.
2. The current Notes keyboard is not visible on hardware. Replace it; do not
   add another local workaround.
3. Storage is cooperative, not a separate task, despite some older target
   diagrams. A storage task is optional Stage 4 work.
4. `WallpaperService` uses LVGL private decoder headers. LVGL upgrades require
   a dedicated compatibility test.
5. There are no host UI tests. Physical checks remain required, with a minimal
   keyboard harness planned for Stage 3.1.
