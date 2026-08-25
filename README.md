# OSEsp32 for ESP32-2432S028

OSEsp32 is a lightweight Windows-inspired application environment for the
ESP32-2432S028 Cheap Yellow Display. The project is built as a normal Arduino
IDE sketch while keeping the implementation in modular C++ files.

The Stage 0 hardware qualification is complete. Development is now on
**Roadmap Stage 1: platform foundation**. The diagnostic UI remains available
while kernel services are introduced underneath it.

## Current capabilities

- Reports chip, CPU, flash, PSRAM, sketch and heap information.
- Tests ILI9341 colors, geometry and orientation.
- Tests backlight brightness control.
- Reads XPT2046 touch through software SPI and prints raw/calibrated values.
- Mounts the TF/microSD slot and verifies write/read/content integrity.
- Tests the RGB LED, speaker and light sensor.
- Stresses display, touch and SD together while tracking heap health.

## Arduino IDE setup

1. Install Arduino IDE 2.x.
2. Add Espressif's ESP32 Boards package URL in Preferences:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Install **esp32 by Espressif Systems** in Boards Manager.
4. Install **LovyanGFX** in Library Manager.
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
small internal SPIFFS area. User applications and documents will eventually
live on the SD card.

`platformio.ini` and the guarded `src/main.cpp` exist only for repeatable
command-line/CI build checks. They do not change the Arduino IDE workflow and
do not create a second `setup()` or `loop()` when Arduino IDE compiles the
sketch.

## Hardware diagnostics

The screen presents six touch buttons. The same tests can be started through
Serial Monitor:

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

## Dependencies

Required now:

- Arduino-ESP32 core
- LovyanGFX
- Built-in `SPI`, `FS` and `SD` libraries from Arduino-ESP32

Planned for later stages:

- LVGL 9 for the shell and window system
- A trimmed Lua runtime for `.yap` applications
- NVS/Preferences for settings
- mbedTLS hashing for package validation

The XPT2046 library is deliberately not required. Touch uses a small software
SPI driver so the SD card can own the second hardware SPI peripheral.

## Project layout

```text
OSEsp32.ino               Arduino entry point only
partitions.csv            4 MiB flash layout
src/board/                board pin map and hardware drivers
src/app/                  top-level application orchestrator
src/kernel/               lifecycle, events, logging, faults and monitoring
src/services/             reusable OS services, including touch calibration
src/diagnostics/          hardware diagnostic application
docs/HARDWARE.md          known and unverified hardware facts
docs/STAGE_0.md           exact test procedure and acceptance criteria
docs/ARCHITECTURE.md      long-term OSEsp32 boundaries
docs/ROADMAP.md           development stages
docs/STAGE_1.md           platform-foundation plan and acceptance checks
```

## Safety

Hardware tests create `/osesp32_sd_test.tmp` and `/osesp32_stress.tmp` on the
SD card and remove them at the end. Back up an important card before testing
and do not remove power or eject the card during an SD write.
