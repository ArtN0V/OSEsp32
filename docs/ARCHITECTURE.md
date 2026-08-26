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

- UI task: LVGL timer, input dispatch and all object mutation.
- Storage task: serialized SD access.
- Application task: Lua VM and application callbacks.
- System task: settings, monitoring and lifecycle.
- Network task: created only while network functionality is requested.

Stages 2 and 3 run the kernel, LVGL and serialized storage service
cooperatively from the Arduino loop. This keeps both LVGL object mutation and
SD access single-owner on the no-PSRAM target. Recovery diagnostics remain
intentionally synchronous. A separate application task is considered in
Stage 4; it must request storage work through the service instead of touching
`SD` directly.

## Stage 3 image policy

- The LVGL `S:` filesystem driver is the only UI-facing path to SD assets.
- BMP and baseline JPEG are decoded incrementally in the viewer; images are
  not copied into a full-screen framebuffer.
- Setting wallpaper is a one-time preprocessing operation. It center-crops or
  pads the source into `/OSEsp32/Wallpapers/desktop.owp`, whose packed `OWP1`
  header is followed by exactly 320x204 RGB565 pixels.
- The OWP decoder serves 20-line areas from SD and retains two strips in RAM.
  Windows therefore restore a ready-to-draw background instead of invoking a
  costly JPEG/BMP decoder on every invalidation.
- PNG and arbitrary resampling are deferred until their heap cost is measured.
- The active OWP path, rotation and interface language live in NVS; image bytes
  remain on SD.

## Stage 3 localization policy

- English is the stable default and Russian is the first optional language.
- Locale is selected before LVGL theme creation. A language change is persisted
  and followed by a controlled restart, avoiding a fragile live recreation of
  every label and keyboard object.
- Both languages use embedded compressed 12/14-pixel ASCII+Cyrillic glyph
  subsets with LVGL Montserrat fallbacks for icon symbols. This ensures a
  language's own name is readable before it is selected. The recovery
  diagnostics UI remains English and independent from the graphical shell
  locale.
- The selected desktop gradient is a small NVS index. It remains independent
  from wallpaper state and becomes visible whenever wallpaper is absent.
