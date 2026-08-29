# OSEsp32 for ESP32-2432S028

OSEsp32 is a lightweight Windows-inspired application environment for the
ESP32-2432S028 Cheap Yellow Display. The project is built as a normal Arduino
IDE sketch while keeping the implementation in modular C++ files.

Stages 0–2 established the hardware, platform foundation and graphical shell;
most Stage 3 storage and personalization work is present. Development is now
on **Stage 4: application runtime**; the first constrained, self-terminating
YAP execution path is implemented. The normal boot opens the LVGL desktop; the
proven diagnostic UI remains available as a recovery mode.

## Current capabilities

- Windows-inspired desktop with icon-style shortcuts, taskbar, Start menu,
  local clock and foreground windows.
- Paged SD file manager, built-in BMP/JPEG viewer and optimized desktop
  wallpapers.
- Windows-style scrollable Settings list for display, language, touch,
  date/time and screen-saver options, including six persistent desktop color
  themes.
- English interface by default plus persistent Russian localization and
  embedded Cyrillic fonts.
- Settings for brightness, touch calibration, wallpaper reset and persistent
  0/180-degree screen rotation.
- Partial LVGL rendering without a full framebuffer.
- Built-in Notes with an SD-backed card gallery, editor, word wrapping,
  touch cursor placement, explicit save and unsaved-change protection.
- Optional resource-releasing screen saver with clock, picture-only and
  Windows-style starfield modes.
- First-boot and Settings-driven five-point touch calibration stored in NVS.
- Recovery hardware diagnostics for display, touch, SD, RGB LED, speaker,
  light sensor and memory/stress testing.

## Arduino IDE setup

1. Install Arduino IDE 2.x.
2. Add Espressif's ESP32 Boards package URL in Preferences:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Install **esp32 by Espressif Systems** in Boards Manager.
4. Install **LovyanGFX** and **lvgl 9.5.0** in Library Manager.
5. The pinned Lua 5.4.9 source is included under `src/vendor/lua549`. To
   reproduce or refresh it, run `python tools/install_lua.py` once from the
   project folder; the script checks the official archive's SHA-256.
6. Open `OSEsp32.ino` from this folder.
7. Select **ESP32 Dev Module**.
8. Use these initial Tools settings:
   - CPU Frequency: `240MHz (WiFi/BT)`
   - Flash Frequency: `40MHz`
   - Flash Mode: `QIO` (use `DIO` if boot is unreliable)
   - Flash Size: `4MB (32Mb)`
   - Upload Speed: `460800` (fall back to `115200` if needed)
   - PSRAM: `Disabled`
9. Compile and upload.
10. Open Serial Monitor at **115200 baud**, line ending optional.

The project-local `partitions.csv` defines two OTA application slots and a
small internal SPIFFS area. On first SD mount, OSEsp32 creates `/OSEsp32/Apps`,
`/OSEsp32/Data`, `/OSEsp32/Notes` and `/OSEsp32/Wallpapers` for applications
and user content.

`platformio.ini` and the guarded `src/main.cpp` exist only for repeatable
command-line/CI build checks. They do not change the Arduino IDE workflow and
do not create a second `setup()` or `loop()` when Arduino IDE compiles the
sketch.

## Desktop and hardware diagnostics

Normal boot opens the desktop. Use **System Info → Hardware Diagnostics** to
store a one-shot recovery request and restart into the hardware test screen.
Press **SHELL** in the diagnostic menu to reboot back into the desktop. A
computer and Serial Monitor are not required; the `u` command remains only as
an optional development shortcut.

The diagnostic screen presents six touch buttons. The same tests can be
started through Serial Monitor:

```text
h  help
a  all automatic tests
d  display
b  backlight
t  touch live view
c  five-point touch calibration
r  reset saved touch calibration
s  SD mount/read/write
o  RGB LED, speaker, light sensor
m  memory and chip report
k  kernel state, events, faults and monitored heap
x  combined stress test
q  return to menu
u  reboot into graphical shell
```

Start with the ordered procedure in [docs/STAGE_0.md](docs/STAGE_0.md). Record
the actual results in its result table; do not change pin mappings based only
on a seller listing.

### Touch calibration

Open **TOUCH**, press **CALIBRATE**, then hold the stylus precisely at the
center of each of the five crosses until the progress bar fills. Lift the
stylus completely between points. The fitted ranges and both axis directions
are saved in NVS and restored automatically after reboot. Send `r` in Serial
Monitor to discard the saved values and return to board defaults.

### Images, wallpaper and rotation

Open **Files** and select a `.bmp`, `.jpg` or `.jpeg` file to view it. Extension
case does not matter, so camera-style `.JPG` files work too. The lightweight
decoder supports baseline JPEG; progressive JPEG must first be exported as
baseline.

**SET WALLPAPER** performs a one-time conversion to
`/OSEsp32/Wallpapers/desktop.owp`: a fixed 320x204 RGB565 file of about 128 KiB.
Large images are center-cropped and small images are centered on a solid fill;
the original file is not changed. During normal drawing, OSEsp32 reads only
the required 20-line strips and keeps the two most recent strips in a roughly
25 KiB RAM cache. This combines fast window redraws with bounded memory use on
the no-PSRAM board. A wallpaper saved by an older build is converted
automatically on its first successful load. The SD card must remain inserted;
Settings can clear the optimized wallpaper.

Settings also offers **ROTATE TO 180** / **ROTATE TO 0**. The choice is saved
and the board restarts so the display and touch transform change together.
Touch calibration remains valid across the two orientations.

### Interface language

Open **Settings → Language** and choose **English** or **Русский**. English is
the first-boot default. The choice is stored in NVS and OSEsp32 restarts to
apply the matching interface font everywhere. The current firmware also
contains a compact Cyrillic keyboard map; its rendering is not yet accepted on
hardware and is being replaced in Stage 3.1. Recovery hardware diagnostics stay
in English so their output remains consistent with the Stage 0 test guide.

The interface embeds one compressed ASCII+Cyrillic font family for both
languages. This also allows the `Русский` choice to render correctly while the
current interface is still English.

### Desktop color

Open **Settings → Display → Desktop Color** to choose Windows blue, Midnight,
Teal, Plum, Slate or Forest. The selected gradient is saved immediately and is
visible whenever wallpaper is disabled. Changing color does not delete the
current wallpaper; use **Clear Wallpaper** to reveal it.

### Notes

Open **Notes** from the desktop or Start menu. The first card creates a note;
saved notes appear as rounded preview cards and the page scrolls when needed.
A note has a bold title and a word-wrapped body. Tap either field to place the
cursor. The toolbar can save or return to the gallery; returning with unsaved
changes offers Save, Don't Save and Cancel. Notes use atomic `.note` files under
`/OSEsp32/Notes`, so a failed replacement does not intentionally destroy the
previous saved version.

Notes now uses the reusable system keyboard described in
[docs/SYSTEM_KEYBOARD.md](docs/SYSTEM_KEYBOARD.md); the old Notes-owned
`lv_keyboard` and its duplicate layouts have been removed. Enter in the title
moves input to the body, Enter in the body inserts a line break, and hiding the
keyboard expands the body to the bottom of the display. Tapping either field
shows the keyboard again.

Saved-note cards have a small circular **X** in their upper-right corner.
Deletion always asks for confirmation and is restricted to `.note` files
directly inside `/OSEsp32/Notes`.

The same component can be checked independently through **System Info →
Keyboard Test**. Verify English and Russian text, case and symbol switching,
Space, Enter, Backspace, cursor arrows and Show/Hide there. Its diagnostic lines
report state, key count, geometry and heap values. The test field is explicitly
single-line to prevent cursor-driven vertical scroll jitter.

The space bar displays **English** or **Русский**. Tap it for a normal space;
hold for about 0.3 seconds and swipe horizontally to change the keyboard
language. The selection remains active for later text fields during the current
session.

### YAP applications — Stage 4 foundation

The first Stage 4 execution slice recognizes `.yap` files in Files, validates
the frozen YAP1 container without loading it all into RAM, and can run its Lua
source inside the requested 16–96 KiB quota. A 200,000-instruction/250 ms guard
stops runaway code. The VM is always closed before OSEsp32 draws the result
window, which shows peak Lua memory and before/after heap diagnostics.

Build the Hello sample with `tools/yap_pack.py` as documented in
[examples/hello_yap/README.md](examples/hello_yap/README.md), copy it to
`/OSEsp32/Apps`, open it in Files and press **RUN**. It should show
`Hello from YAP!` and `after close: 0 B`. The exact format is in
[docs/YAP1_FORMAT.md](docs/YAP1_FORMAT.md). Lua runtime candidates and the
required target measurements are tracked in
[docs/LUA_RUNTIME_SPIKE.md](docs/LUA_RUNTIME_SPIKE.md).

Run `python tools/build_yap_examples.py` to create Hello plus controlled
compile-error, missing-entry, out-of-memory and infinite-loop packages. Their
expected results are documented in
[examples/yap_runtime_tests/README.md](examples/yap_runtime_tests/README.md).
The current application API contains only `osesp32.ui.label(text)`; application
file access, persistent events and fullscreen/exclusive launch are not present
yet.

### Date, time and screen saver

**Settings → Date & time** currently sets the clock and UTC offset manually.
`DateTimeService::setUtc()` is the future synchronization boundary, but Stage 3
does not start Wi-Fi. The CYD has no battery-backed real-time clock: time runs
normally while powered, but after a complete power loss it resumes from the
last manually saved value and cannot account for the powered-off interval.

**Settings → Screen saver** enables it and selects an inactivity delay. Use the
left/right arrows to choose a centered clock/date panel, a picture-only mode,
or a Windows-style starfield flying outward from the center. Picture mode uses
any supported SD image and decodes it only when the saver opens. When the saver
closes, its LVGL object tree and exact image cache entry are discarded. The
saver is suppressed in fullscreen applications such as the note editor. If
the selected picture is unavailable, picture mode safely displays black.

## Dependencies

Required now:

- Arduino-ESP32 core
- LovyanGFX
- LVGL 9.5.0
- Built-in `SPI`, `FS` and `SD` libraries from Arduino-ESP32
- Built-in `Preferences` for calibration and one-shot boot settings
- Pinned trimmed Lua 5.4.9 source for `.yap` execution

The generated Cyrillic glyph data in `src/ui/OSEsp32Font12.c`,
`src/ui/OSEsp32Font14.c` and `src/ui/OSEsp32Font16Bold.c` is derived from Noto
Sans, distributed under the Apache License 2.0. See
[docs/THIRD_PARTY.md](docs/THIRD_PARTY.md).

Planned for later stages: mbedTLS hashing/signatures for packages.

The XPT2046 library is deliberately not required. Touch uses a small software
SPI driver so the SD card can own the second hardware SPI peripheral.

## Project layout

```text
OSEsp32.ino               Arduino entry point only
partitions.csv            4 MiB flash layout
build_opt.h / lv_conf.h   Arduino IDE LVGL configuration
src/board/                board pin map and hardware drivers
src/app/                  top-level application orchestrator
src/kernel/               lifecycle, events, logging, faults and monitoring
src/services/             reusable OS services, including touch calibration
src/ui/                   LVGL display and calibrated pointer adapter
src/shell/                desktop, windows and built-in applications
src/runtime/              constrained YAP Lua execution
src/vendor/lua549/        pinned trimmed Lua 5.4.9 source
src/diagnostics/          hardware diagnostic application
docs/HARDWARE.md          known and unverified hardware facts
docs/STAGE_0.md           exact test procedure and acceptance criteria
docs/ARCHITECTURE.md      long-term OSEsp32 boundaries
docs/PROJECT_MAP.md       actual source ownership, boot flow and persistent data
docs/AUDIT.md             verified strengths, risks and refactoring decisions
docs/SYSTEM_KEYBOARD.md   reusable keyboard architecture and hardware gate
docs/ROADMAP.md           development stages
docs/STAGE_1.md           platform-foundation plan and acceptance checks
docs/STAGE_2.md           graphical-shell plan and acceptance checks
docs/STAGE_3.md           storage and personalization plan and checks
docs/STAGE_3_1.md         stabilization plan before the YAP runtime
docs/STAGE_4.md           sandboxed YAP runtime and application storage plan
```

## Safety

Hardware tests create `/osesp32_sd_test.tmp` and `/osesp32_stress.tmp` on the
SD card and remove them at the end. Back up an important card before testing
and do not remove power or eject the card during an SD write.
