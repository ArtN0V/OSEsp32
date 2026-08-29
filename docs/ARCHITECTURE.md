# OSEsp32 architecture decisions

This document contains both current Stage 3.1 decisions and explicitly planned
Stage 4 boundaries. For the exact source that exists today, start with
`PROJECT_MAP.md`. Types such as `AppStorageService`, the Lua runtime and the
application/network tasks are plans, not implemented components.

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

The directory layout already follows the first five names, but the current
implementation is not fully separated: `DesktopShell` constructs most services
and contains the window manager plus all built-in graphical applications.
Stage 3.1 extracts shared system overlays first instead of attempting a broad
rewrite.

## Current composition and target composition

Current:

```text
OSEsp32App
  -> SystemKernel
  -> BootModeService
  -> DesktopShell
       -> LvglPort + Storage/Settings/Notes/Wallpaper/Time services
       -> desktop + windows + built-in apps + overlays
  OR DiagnosticsApp
```

Incremental target:

```text
OSEsp32App (composition root)
  -> kernel and hardware-facing services
  -> graphical environment
       -> LvglPort
       -> system overlays (keyboard, dialogs, file picker, exit control)
       -> DesktopShell/window manager
       -> built-in app controllers
       -> one optional YAP runtime
```

The target keeps a single LVGL-mutating loop. Moving ownership upward does not
authorize background tasks to touch LVGL.

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

`.yap` means Yellow Application Package. Version 1 is now frozen as the single,
uncompressed, seekable container in `YAP1_FORMAT.md`, with:

- fixed `YAP1` header;
- section table;
- compact manifest;
- Lua program section;
- icon and resource sections;
- CRC32, with SHA-256/signatures added later.

The manifest also declares, without granting by itself:

- application identifier and OSEsp32 API version;
- requested launch mode: `windowed`, `fullscreen` or `exclusive`;
- requested Lua memory budget and required capabilities;
- file classes the application can open or create, such as `image/bmp`.

OSEsp32 policy may reject a package or select a stricter launch mode. A manifest
request never becomes direct hardware or filesystem access.

Native Xtensa code from SD is explicitly out of scope for version 1 because a
classic ESP32 cannot isolate a faulty native application from the OS.

The current `YapPackageService` implements only the trust boundary before the
runtime: it reads bounded header/section blocks, computes package and section
CRC32 in 256-byte chunks, rejects malformed bounds/overlap/duplicates and
validates the fixed manifest. The Files page may display validated metadata,
but no `LUAS` byte reaches an interpreter yet. Runtime candidate measurements
and the allocator/hook gate are defined in `LUA_RUNTIME_SPIKE.md`.

## Threading model

- UI task: LVGL timer, input dispatch and all object mutation.
- Storage task: serialized SD access.
- Application task: Lua VM and application callbacks.
- System task: settings, monitoring and lifecycle.
- Network task: created only while network functionality is requested.

Only the cooperative Arduino/UI loop exists today. The storage, application,
system and network task bullets above describe possible Stage 4/7 separation,
not current FreeRTOS tasks.

Stages 2 and 3 run the kernel, LVGL and serialized storage service
cooperatively from the Arduino loop. This keeps both LVGL object mutation and
SD access single-owner on the no-PSRAM target. Recovery diagnostics remain
intentionally synchronous. A separate application task is considered in
Stage 4; it must request storage work through the service instead of touching
`SD` directly.

## Application lifecycle and memory reclamation

- `windowed` keeps the shell and is intended for small utilities.
- `fullscreen` covers the desktop but may retain its objects and caches.
- `exclusive` saves only a compact shell state, closes shell-owned files,
  destroys desktop LVGL objects, drops wallpaper/image caches and verifies a
  minimum free-heap and largest-block threshold before creating the VM.
- Kernel, display/touch ports, serialized storage, memory monitor, watchdog and
  a system-owned exit path always remain alive. An application cannot replace
  or hide that recovery mechanism.
- Lua is created with a quota-aware allocator. An instruction-count hook gives
  control back to the system and lets it enforce time limits. On exit, the OS
  stops callbacks, closes every application file handle, destroys application
  UI, calls `lua_close`, drops application caches, checks for a memory leak and
  rebuilds the shell from NVS and the compact saved state.
- The shell is reconstructed, not serialized into RAM. This deliberately
  trades a short return delay for a larger contiguous block while an exclusive
  application is running.

General virtual memory is out of scope. ESP32 pointers cannot transparently
address SD data, and random swap traffic would be slow and fragile. Large data
must instead use streamed resources or an explicit page-oriented API whose
objects are handles, not pointers.

## Application storage and capabilities

Everything in this section is a Stage 4 contract. The current
`StorageService` is a trusted built-in shell service, not yet the capability
layer described below.

All application I/O passes through an `AppStorageService` layered over the
single-owner `StorageService`. Lua never receives `File`, `FILE*`, LVGL drive
paths or raw SD paths.

Storage namespaces and grants are:

- `app:/` — read-only package resources inside the active `.yap`;
- `data:/` — read/write private directory
  `/OSEsp32/Data/<application-id>/`, available only to that application;
- user documents — no ambient directory access. A system-owned Open or Save
  dialog returns an opaque capability for the exact file selected by the user.
  A capability carries allowed operations and expires when the app exits.

Version 1 file operations are bounded `open`, `read`, `write`, `seek`, `size`,
`flush`, `close`, `stat` and private-directory listing. The service canonicalizes
paths, rejects traversal/control characters, limits path and filename length,
caps open handles per application, checks free-space reserve and performs I/O
in small cooperative chunks. Every operation returns a stable error such as
`not_found`, `permission_denied`, `storage_removed`, `no_space`, `io_error` or
`quota_exceeded`.

Application write modes explicitly distinguish create-new, truncate, append
and transactional replace. They do not reuse the LVGL viewer bridge's generic
write mode or its heap-allocated `File` handles. The application layer uses a
fixed handle table and a stricter component-by-component canonicalizer.

Save/replace is transactional at the service level: write and flush a sibling
temporary file, preserve an existing destination as a backup, rename the
temporary file into place, then remove the backup. Boot/mount recovery resolves
recognized `.tmp`/`.bak` remnants. Applications cannot implement this sequence
with unrestricted rename/delete calls.

On SD removal, all affected handles become invalid and pending calls return
`storage_removed`. The runtime pauses the app and presents a system-owned
Retry/Close decision. It never silently redirects user documents to internal
flash. Package code already resident in RAM is not treated as proof that the
removed card or a newly inserted card is the same volume.

## Paint reference architecture

Paint uses the same public APIs expected of third-party `.yap` applications:

- Open/Save dialogs grant one BMP document at a time; the manifest requests
  BMP read/create/replace capability but does not grant arbitrary SD access.
- BMP headers and rows are decoded/encoded incrementally. Initial import covers
  uncompressed 16/24/32-bit BMP. Save As emits interoperable uncompressed
  24-bit BGR rows with required four-byte padding through transactional replace.
- When both Viewer and Paint handle BMP, the shell owns an **Open with** choice
  and optional default association. A package cannot silently take over an
  existing file type.
- Pixel storage belongs to a native `Canvas` service. Lua sees drawing methods
  and events, never a table containing every pixel.
- First preference is a memory-measured exclusive canvas: RGB565 only if a
  safe contiguous allocation remains, otherwise an indexed canvas. A bounded
  tile cache backed by `data:/tmp/` is a later fallback, not general swap.
- The editor tracks a dirty flag. Close, SD removal and write failure preserve
  the in-memory drawing where possible and ask the user before discarding it.

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

## Stage 3 notes policy

- Notes are built-in documents, not unrestricted application storage. They
  live under `/OSEsp32/Notes` and are accessed through `NotesService` over the
  single-owner `StorageService`.
- The gallery keeps only bounded summaries in RAM. One editor owns one bounded
  title/body buffer; no note is mapped directly from SD.
- Save uses the storage service's temporary-file, backup and rename sequence.
  The editor retains dirty text and refuses to pretend it saved if the SD card
  is missing or a write fails.
- The editor is fullscreen and sets the same shell lifecycle flag that future
  fullscreen `.yap` applications use. This suppresses the screen saver without
  coupling the saver to Notes specifically.
- Text entry is not accepted on hardware. Notes must adopt the system-owned
  input component in `SYSTEM_KEYBOARD.md`; it may not keep an application-owned
  keyboard object.

## Stage 3 clock and screen-saver policy

- `DateTimeService` owns UTC, elapsed-time accounting and a local UTC offset.
  Manual entry is the current source; a future network service will submit NTP
  results only through `setUtc()`.
- NVS stores a checkpoint, not powered-off elapsed time. Without a battery RTC
  or network synchronization, the clock cannot stay accurate across complete
  power loss.
- Screen-saver activation uses LVGL's display inactivity counter. Fullscreen
  activity suppresses it, and a wake event resets activity.
- Saver mode is a persistent enum: clock, picture-only or starfield. A picture
  keeps its original SD path and may decode slowly on entry; it deliberately
  does not use the desktop OWP cache. Starfield allocates a small star array
  only while active and draws all streaks through one custom LVGL layer without
  a framebuffer or per-star widgets. On wake, the array, complete saver object
  tree and exact image cache entry are discarded so the steady-state shell does
  not pay their memory cost.

## System UI overlay policy

- Keyboard, confirmation dialogs, future Open/Save picker and exclusive-app
  exit control are OS-owned overlays. An application requests them and never
  creates, deletes or retains their LVGL roots.
- Each overlay has one owner, an idempotent show/hide contract and an explicit
  shutdown path. Event callbacks must not synchronously delete their current
  event target.
- Overlay stacking is defined centrally. Application content receives the
  remaining work area when a docked overlay such as the keyboard becomes
  visible.
- The first extraction is `SystemKeyboard`, implemented as a directly
  controlled `lv_buttonmatrix` with an application-neutral input-client
  adapter. See `SYSTEM_KEYBOARD.md`.

## Current trusted-storage policy

- `StorageService` is the only shell-mode owner of Arduino `SD`; diagnostic
  mode uses a separate driver only because the two modes are mutually
  exclusive.
- Every public path operation uses one bounded component canonicalizer. It
  accepts UTF-8 bytes but rejects traversal components, control characters,
  backslashes, reserved separators and overlong output.
- Rename is non-overwriting. Transactional replacement first preserves the
  existing destination as `.bak`, installs the completed temporary file, then
  removes the backup. FAT is not fully power-fail-safe; recognized remnant
  recovery is still required before third-party document replacement ships.
