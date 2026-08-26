# OSEsp32 roadmap

## Stage 0 — hardware qualification

Validate identity, memory, TFT, backlight, touch, SD, RGB LED, speaker, light
sensor and combined operation. Exit criterion: completed Stage 0 result table.

## Stage 1 — platform foundation

Keep the proven board drivers, introduce fixed-memory logging, event queues,
lifecycle, fault reporting and continuous heap monitoring. Move touch
calibration into a reusable system service shared by diagnostics, first-run
setup and Settings.

## Stage 2 — graphical shell

Integrate LVGL partial rendering and build the Windows-inspired theme, desktop,
taskbar, Start menu, dialogs and touch keyboard.

## Stage 3 — storage, files and personalization (current)

Build serialized SD service, file manager, file associations, application data
directories and safe behavior when a card is removed or damaged. Add streamed
BMP/JPEG viewing, SD-backed desktop wallpaper and persistent 0/180-degree
display rotation. Keep PNG and image scaling out of the first version because
this board has no PSRAM.

## Stage 4 — application runtime

Integrate constrained Lua, define OSEsp32 API version 1, enforce memory/time
limits and load the first external Hello World `.yap` from SD.

## Stage 5 — desktop SDK

Create templates, a `.yap` packer, simulator-oriented APIs and example apps:
calculator, notes and a port of the existing game. Extend the built-in image
viewer with scaling and optional PNG support only if memory tests permit it.

## Stage 6 — stabilization

Add watchdog handling, leak tests, crash logs, permissions, package validation
and repeated launch/close endurance tests.

## Stage 7 — optional connectivity

Add on-demand Wi-Fi, HTTP API, OTA and an application repository only after the
offline system is stable within its memory budget.
