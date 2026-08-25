# YellowOS roadmap

## Stage 0 — hardware qualification (current)

Validate identity, memory, TFT, backlight, touch, SD, RGB LED, speaker, light
sensor and combined operation. Exit criterion: completed Stage 0 result table.

## Stage 1 — platform foundation

Keep the proven board drivers, introduce logging, event queues, lifecycle and
fault reporting, then establish a memory budget from measured Stage 0 values.

## Stage 2 — graphical shell

Integrate LVGL partial rendering and build the Windows-inspired theme, desktop,
taskbar, Start menu, dialogs and touch keyboard.

## Stage 3 — storage and files

Build serialized SD service, file manager, file associations, application data
directories and safe behavior when a card is removed or damaged.

## Stage 4 — application runtime

Integrate constrained Lua, define Yellow API version 1, enforce memory/time
limits and load the first external Hello World `.yap` from SD.

## Stage 5 — desktop SDK

Create templates, a `.yap` packer, simulator-oriented APIs and example apps:
calculator, notes, image viewer and a port of the existing game.

## Stage 6 — stabilization

Add watchdog handling, leak tests, crash logs, permissions, package validation
and repeated launch/close endurance tests.

## Stage 7 — optional connectivity

Add on-demand Wi-Fi, HTTP API, OTA and an application repository only after the
offline system is stable within its memory budget.
