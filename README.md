# OSEsp32 for ESP32-2432S028

OSEsp32 is a lightweight Windows-inspired application environment for the
ESP32-2432S028 Cheap Yellow Display. The project is built as a normal Arduino
IDE sketch while keeping the implementation in modular C++ files.

Stages 0–2 established the hardware, platform foundation and graphical shell.
Development is now on **Roadmap Stage 3: storage, files and personalization**.
The normal boot opens the LVGL desktop; the proven diagnostic UI remains
available as a recovery mode.

## Current capabilities

- Windows-inspired desktop with icon-style shortcuts, taskbar, Start menu,
  uptime clock and foreground windows.
- Paged SD file manager, built-in BMP/JPEG viewer and optimized desktop
  wallpapers.
- Windows-style Settings list for display, language and touch options,
  including six persistent desktop color themes.
- English interface by default plus persistent Russian localization and a
  Cyrillic on-screen keyboard.
- Settings for brightness, touch calibration, wallpaper reset and persistent
  0/180-degree screen rotation.
- Compact on-screen keyboard and partial LVGL rendering without a full
  framebuffer.
- First-boot and Settings-driven five-point touch calibration stored in NVS.
- Recovery hardware diagnostics for display, touch, SD, RGB LED, speaker,
  light sensor and memory/stress testing.

## Arduino IDE setup

1. Install Arduino IDE 2.x.
2. Add Espressif's ESP32 Boards package URL in Preferences:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Install **esp32 by Espressif Systems** in Boards Manager.
4. Install **LovyanGFX** and **lvgl 9.5.0** in Library Manager.
5. Open `OSEsp32.ino` from this folder.
6. Select **ESP32 Dev Module**.
7. Use these initial Tools settings:
   - CPU Frequency: `240MHz (WiFi/BT)`
   - Flash Frequency: `40MHz`
   - Flash Mode: `QIO` (use `DIO` if boot is unreliable)
   - Flash Size: `4MB (32Mb)`
   - Upload Speed: `460800` (fall back to `115200` if needed)
   - PSRAM: `Disabled`
8. Compile and upload.
9. Open Serial Monitor at **115200 baud**, line ending optional.

The project-local `partitions.csv` defines two OTA application slots and a
small internal SPIFFS area. On first SD mount, OSEsp32 creates `/OSEsp32/Apps`,
`/OSEsp32/Data` and `/OSEsp32/Wallpapers` for future applications and content.

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
apply the matching interface font everywhere. Russian mode also replaces the
text keyboard with a compact Cyrillic layout; the `1#` key still opens the
standard number/symbol page. Recovery hardware diagnostics intentionally stay
in English so their output remains consistent with the Stage 0 test guide.

The interface embeds one compressed ASCII+Cyrillic font family for both
languages. This also allows the `Русский` choice to render correctly while the
current interface is still English.

### Desktop color

Open **Settings → Display → Desktop Color** to choose Windows blue, Midnight,
Teal, Plum, Slate or Forest. The selected gradient is saved immediately and is
visible whenever wallpaper is disabled. Changing color does not delete the
current wallpaper; use **Clear Wallpaper** to reveal it.

## Dependencies

Required now:

- Arduino-ESP32 core
- LovyanGFX
- LVGL 9.5.0
- Built-in `SPI`, `FS` and `SD` libraries from Arduino-ESP32
- Built-in `Preferences` for calibration and one-shot boot settings

The generated Cyrillic glyph data in `src/ui/OSEsp32Font12.c` and
`src/ui/OSEsp32Font14.c` is derived from Noto Sans, distributed under the
Apache License 2.0. See [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md).

Planned for later stages:

- A trimmed Lua runtime for `.yap` applications
- mbedTLS hashing for package validation

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
src/diagnostics/          hardware diagnostic application
docs/HARDWARE.md          known and unverified hardware facts
docs/STAGE_0.md           exact test procedure and acceptance criteria
docs/ARCHITECTURE.md      long-term OSEsp32 boundaries
docs/ROADMAP.md           development stages
docs/STAGE_1.md           platform-foundation plan and acceptance checks
docs/STAGE_2.md           graphical-shell plan and acceptance checks
docs/STAGE_3.md           storage and personalization plan and checks
```

## Safety

Hardware tests create `/osesp32_sd_test.tmp` and `/osesp32_stress.tmp` on the
SD card and remove them at the end. Back up an important card before testing
and do not remove power or eject the card during an SD write.
