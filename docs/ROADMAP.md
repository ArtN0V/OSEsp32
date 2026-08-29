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

## Stage 3 — storage, files and personalization (feature-complete, not accepted)

Build serialized SD service, file manager, file associations, application data
directories and safe behavior when a card is removed or damaged. Add streamed
BMP/JPEG viewing, preprocessed strip-cached desktop wallpaper, persistent
0/180-degree display rotation, a Windows-style Settings category list and
English/Russian localization. Keep PNG and image scaling out of the first
version because this board has no PSRAM. Replace Text Input with SD-backed
Notes, make Settings scrollable, add a manual date/time service prepared for a
later NTP source, and add a resource-releasing clock, picture-only and animated
starfield screen saver that is disabled while fullscreen applications run.

The feature set is present. The replacement system keyboard is visible on the
target board and has been connected to Notes; the remaining Stage 3.1 lifecycle
and memory checks still block final acceptance.

## Stage 3.1 — stabilization and system UI extraction (current)

Replace the Notes-specific keyboard with a reusable system overlay built from
a directly controlled button matrix and prove it first in an isolated hardware
test. Harden path canonicalization and transactional replacement, document the
actual code ownership, then begin extracting shared overlays from the monolithic
`DesktopShell`. See [STAGE_3_1.md](STAGE_3_1.md) and
[SYSTEM_KEYBOARD.md](SYSTEM_KEYBOARD.md).

Current checkpoint: the reusable `SystemKeyboard` passed its initial isolated
on-board check, and Notes now consumes it through a textarea adapter. The old
Notes-owned widget has been removed. Repeated lifecycle, rotation and memory
checks remain before Stage 3.1 can close.

## Stage 4 — application runtime

Status: **foundation in progress**. YAP1 is frozen and its streaming validator,
host packer, Hello sample and Files metadata view are implemented. Package code
is not executed until the Lua footprint/quota spike is completed on hardware.

Build the first complete `.yap` execution path rather than only embedding Lua:

- freeze the `YAP1` manifest and package reader, including API version,
  requested launch mode, memory budget and permissions;
- integrate one constrained Lua VM with a quota allocator and instruction/time
  hooks;
- implement `windowed`, `fullscreen` and `exclusive` launch modes. Exclusive
  mode destroys the desktop and its caches, retains only kernel/display/touch,
  storage, monitoring and a system-owned exit overlay, then reconstructs the
  shell after `lua_close`;
- expose capability-based application storage instead of Arduino `SD`: private
  per-app data plus user-selected document handles, bounded/chunked I/O,
  explicit errors and atomic save/replace;
- load Hello World and a file round-trip test from SD, then prove repeated
  launch/close returns heap and the largest free block to a stable baseline.

Stage 4 may not begin until the Stage 3.1 system keyboard and shared-overlay
lifecycle pass on hardware; YAP applications must consume those services rather
than adding their own keyboards or file dialogs.

Do not implement a general SD swap file. Code and resources may be streamed,
and an explicit paged-data API can be added later, but Lua heap and native UI
objects must remain in real RAM. See [STAGE_4.md](STAGE_4.md).

## Stage 5 — desktop SDK

Create templates, a `.yap` packer, simulator-oriented APIs and example apps:
calculator, notes, Paint and a port of the existing game. Paint is the storage
and memory stress reference: shell-owned Open/Save dialogs, BMP import/export,
atomic replacement, SD-removal recovery and a native canvas that never exposes
pixels as Lua tables. Start with a measured exclusive RGB565 or indexed canvas;
add a tiled SD-backed canvas only if the RAM version cannot meet the budget.
Add Windows-like **Open with** handling and an optional saved default when BMP
has both the built-in Viewer and Paint associations.
Extend the built-in image viewer with scaling and optional PNG support only if
memory tests permit it.

## Stage 6 — stabilization

Add watchdog handling, leak tests, crash logs, permission review, package
validation and repeated launch/close endurance tests. Add interrupted-save and
SD removal/reinsertion tests, recovery of `.tmp`/`.bak` transactions, storage
quotas and optional explicitly paged collections for datasets larger than RAM.

## Stage 7 — optional connectivity

Add on-demand Wi-Fi, HTTP API, OTA and an application repository only after the
offline system is stable within its memory budget.
