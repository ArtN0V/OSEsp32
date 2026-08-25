# OSEsp32 architecture decisions

## Product boundary

OSEsp32 is an embedded application environment, not a replacement for the
ESP32 ROM bootloader or FreeRTOS. Arduino-ESP32 supplies ESP-IDF and FreeRTOS;
OSEsp32 supplies the hardware abstraction, services, graphical shell and
sandboxed application runtime.

## Layers

1. `board`: the only layer that knows CYD GPIO numbers and controller details.
2. `kernel`: event queues, lifecycle, timers, logging and memory policy.
3. `services`: storage, settings, time, networking and package management.
4. `ui`: LVGL port, theme, compositor and window manager.
5. `shell`: desktop, Start menu, taskbar, dialogs and system applications.
6. `runtime`: one constrained Lua VM for the active `.yap` application.

## Non-negotiable constraints

- No full-screen double RGB565 framebuffer on a no-PSRAM device.
- LVGL uses partial 16-bit buffers, initially two 320x20-line buffers.
- Only the UI task may mutate LVGL objects; other tasks send events.
- Only one third-party application VM is active initially.
- Application assets are streamed from SD where practical.
- Applications never receive raw pointers, GPIO access or unrestricted files.
- System settings live in NVS; applications and user data live on SD.
- A failing SD card must not prevent the built-in recovery shell from booting.

## Application format

`.yap` means Yellow Application Package. Version 1 is planned as a single,
uncompressed, seekable container with:

- fixed `YAP1` header;
- section table;
- compact manifest;
- Lua program section;
- icon and resource sections;
- CRC32, with SHA-256/signatures added later.

Native Xtensa code from SD is explicitly out of scope for version 1 because a
classic ESP32 cannot isolate a faulty native application from the OS.

## Threading model

- UI task: LVGL timer and input dispatch.
- Storage task: serialized SD access.
- Application task: Lua VM and application callbacks.
- System task: settings, monitoring and lifecycle.
- Network task: created only while network functionality is requested.

The initial diagnostics are intentionally simpler and mostly synchronous. They
validate hardware before this task model is introduced in Stage 1.
