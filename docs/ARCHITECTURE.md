# OSEsp32 architecture decisions

This document contains current decisions, implemented Stage 4 boundaries and
later target architecture. For
the exact source that exists today, start with `PROJECT_MAP.md`.
`YapRuntimeService`, `AppLifecycle`, `AppStorageService`, `YapUiHost` and
`FileAssociationService` exist. Separate application/network tasks remain plans.

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

The current `YapPackageService` implements the trust boundary before the
runtime: it reads bounded header/section blocks, computes package and section
CRC32 in 256-byte chunks, rejects malformed bounds/overlap/duplicates and
validates the fixed manifest. `YapRuntimeService` then streams only the verified
`LUAS` section into a new Lua 5.4.9 state, registers the small safe API, calls
the manifest entry and destroys the state. Runtime measurements and remaining
hardware gates are defined in `LUA_RUNTIME_SPIKE.md`.

The host owns one coroutine, resumed from the UI loop. Every 1,000 Lua
instructions the count hook yields; quotas are checked outside Lua so `pcall`
cannot swallow termination. `osesp32.sleep(1..60000)` yields until a deadline,
allowing long-lived cooperative apps. `osesp32.ui.label(text)` copies up to 96
bytes to a system-owned label; no LVGL pointers or UI callbacks reach Lua.
`osesp32.exit()` yields permanently and requests successful termination. It
does not return to Lua, even through pcall; the host closes the VM and restores
the desktop without opening the diagnostic report. Saving or confirmation
belongs before this call. Six API 1.0/1.1 system-owned buttons emit bounded
queued IDs. API 1.2 adds a fixed native model of at most 24
labels/buttons/toggles/text fields/lists, 12 aggregate list rows, four
cooperative timers and eight queued events. Integer IDs cross the boundary;
LVGL pointers do not. Lua awaits all input through `ui.wait()` instead of
running inside LVGL callbacks.
Text/file requests suspend the coroutine; response allocation occurs inside
protected `lua_resume`, so an OOM response cannot panic outside Lua's boundary.

Only windowed apps have the OS title bar and EXIT button. Fullscreen and
exclusive have no visible system exit control. A 2-second hold inside the
top-left 32x32 screen pixels requests emergency cancellation; it uses calibrated,
rotation-adjusted input independently of Lua. An initial release is required
and leaving the corner cancels the hold. Touch is consumed until release on
teardown so it cannot activate the restored desktop. No LVGL object is deleted
inside its own event callback.

App-created coroutines, metatable manipulation, table.sort and string pattern
operations are not exposed in this slice. These would permit uncontrolled
execution during teardown or non-yieldable native callbacks. Loading source
and native library operations remain synchronous; the hook is not a hard
real-time preemptor for C code.

## Threading model

- UI/application loop: LVGL, input, lifecycle, storage requests and one Lua
  resume slice; this is the only implemented execution context here.
- Optional future workers: serialized storage and network work that return
  messages and never mutate LVGL or resume Lua directly.

All implemented stages run kernel, LVGL, serialized storage and bounded Lua
slices cooperatively from the Arduino loop. This keeps object mutation and SD
access single-owner on the no-PSRAM target. Recovery diagnostics remain
intentionally synchronous. A later worker must request storage through the
service and return messages; it cannot touch LVGL or expose Arduino `SD` to Lua.

## Application lifecycle and memory reclamation

Implemented by `AppLifecycle` and `DesktopShell::updateYapSession`:
`Idle → Preparing → Running → Stopping → RestoringShell → Idle`.
Each phase advances from the main loop. Errors and EXIT take the same teardown
path. The three manifest modes are honored; insufficient memory rejects launch
instead of silently changing the mode. Before VM creation, the host requires
the requested quota plus 24 KiB free reserve and a 12 KiB largest free block.
This is an admission check, not a guarantee against subsequent allocation
failure; the allocator still enforces the hard Lua quota.

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

The current exclusive implementation releases desktop objects, the keyboard
tree, decoder cache entries and dynamically allocated wallpaper strips. Fixed
Notes/File-service buffers, LVGL partial buffers and kernel state stay resident.
File-manager path/page remain in the shell; settings remain in NVS. On restore,
the desktop is rebuilt. An explicit app exit returns directly to the desktop;
other completion paths open a scrollable execution report. A normal
return, error and EXIT close the VM. Detected SD removal instead invalidates
all file handles, pauses Lua and offers Retry/Close. Retry revalidates the package
section table/CRCs, repairs transactions and starts a new file session; it never
revives an old handle or silently re-grants the launch document.

General virtual memory is out of scope. ESP32 pointers cannot transparently
address SD data, and random swap traffic would be slow and fragile. Large data
must instead use streamed resources or an explicit page-oriented API whose
objects are handles, not pointers.

## Application storage and capabilities

The Stage 4 contract below is implemented. `StorageService` remains the trusted
built-in SD owner; `AppStorageService` is the capability layer above it.

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

Save/replace is transactional at the service level: stage into one of four
system-owned `/OSEsp32/Transactions/<slot>.data` files, retain a checksummed
target journal and preserve the destination in `<slot>.old` before rename.
Only `close(handle)` commits; close(false), cancellation and app exit abort.
Boot/new launch or paused-session Retry recovers known remnants; damaged
journals and unexplained backups are preserved and block reuse. The previous
built-in Notes/OWP `.bak` format has narrowly scoped recovery too. FAT hardware
power-fail atomicity is not guaranteed. Apps have no unrestricted rename/delete.

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
- Target measurements select one exclusive indexed 4-bit Canvas: a 32,704-byte
  buffer and fixed 16-color palette. RGB565 cannot allocate, while indexed
  8-bit leaves only a 20,468-byte largest block. A bounded tile cache backed by
  `data:/tmp/` is a later fallback, not general swap.
- The editor tracks a dirty flag. Close, SD removal and write failure preserve
  the in-memory drawing where possible and ask the user before discarding it.

The temporary API 1.3 `canvas.probe/release` pair remains accepted only in exclusive
mode, and `YapUiHost` owns its LVGL object and one 320x204 draw buffer. It tests
RGB565, indexed 8-bit and indexed 4-bit with dirty rectangles and reports heap,
largest-block and timing data. API 1.4 adds a bounded I4-only drawing Canvas,
clear/line commands and queued coordinate touch events. API 1.5 adds a fixed
scanline BMP bridge to document handles. It still exposes no pixel pointer or
Lua pixel table. Normal/emergency teardown releases the Canvas before rebuilding
the shell; SD removal cancels file transfer but retains a Paint Canvas through
the Retry/Close decision. API 1.3 stays a diagnostic contract.
The display object is deliberately a plain `lv_image`, not LVGL 9.5.0's
`lv_canvas`: the pinned Canvas destructor drops the wrong cache key. Both image
caches are disabled in this build. Teardown detaches the variable image source,
deletes the image, and only then destroys its draw buffer, so invalidation never
observes freed descriptor storage.

`ResetDiagnostics` keeps one small breadcrumb in RTC memory. Canvas ownership
marks allocation, fill, animation and release boundaries; boot captures the
previous marker and `esp_reset_reason()`, and System Info exposes both. This is
crash triage rather than persistent settings or an RTC clock, and complete
power removal may erase it.
BMP import and drawing additionally mark read, ready, input, draw and redraw
boundaries so a watchdog reset can be separated from an ordinary app exit.

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
- Notes uses the system input component in `SYSTEM_KEYBOARD.md`; basic text
  entry passed the user's board check. Repetition/rotation/heap acceptance
  remains pending; there is no Notes-owned keyboard object.

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

- Keyboard, confirmation dialogs, Stage 4 Open/Save picker and exclusive-app
  exit control are OS-owned overlays. An application requests them and never
  creates, deletes or retains their LVGL roots.
- Each overlay has one owner, an idempotent show/hide contract and an explicit
  shutdown path. Event callbacks must not synchronously delete their current
  event target.
- Overlay stacking is defined centrally. Application content receives the
  remaining work area when a docked overlay such as the keyboard becomes
  visible.
- `SystemKeyboard` is implemented as a directly
  controlled `lv_buttonmatrix` with an application-neutral input-client
  adapter; `YapUiHost` owns YAP buttons, text and file-dialog roots. See
  `SYSTEM_KEYBOARD.md`.

## Current trusted-storage policy

- `StorageService` is the only shell-mode owner of Arduino `SD`; diagnostic
  mode uses a separate driver only because the two modes are mutually
  exclusive.
- Bounded range reads and writes reuse one `File` stream per direction instead
  of reopening FAT for each 512-byte chunk. They close and flush on handle
  close, explicit flush, mutation, raw-card probe and removal, preventing long
  BMP transfers from fragmenting the internal heap. The writer maintains its
  own logical length because the SD library can report the pre-flush size while
  buffered blocks are still pending.
- Every public path operation uses one bounded component canonicalizer. It
  accepts UTF-8 bytes but rejects traversal components, control characters,
  backslashes, reserved separators and overlong output.
- Rename is non-overwriting. Transactional replacement first preserves the
  existing destination as `.bak`, installs the completed temporary file, then
  removes the backup. Known Notes/OWP backups are recovered before load/save;
  third-party transactions use their separate journaled capability service.

## Stage 4 limits and trust assumptions

See `YAP_API.md` for the exact callable contract. No arbitrary GUI tree or native
pointer is exposed. API 1.2 is bounded to 24 host-owned widgets, 12 aggregate
list rows, four timers and eight queued events; API 1.0/1.1 retains its six
fixed buttons. API 1.3 exposes the exclusive system-owned Canvas measurement;
API 1.4 adds bounded I4 create/clear/line commands and coordinate touch events.
API 1.5 adds scanline BMP load/save against capability handles; the host owns a
fixed 1,280-byte work buffer, reads oversized rows in bounded windows and never
creates a second Canvas. Four handles and
512-byte storage transfers bound native memory
independently of Lua's quota. Files can grow to
1 MiB with 128 KiB card reserve; append copies at most 4 KiB synchronously.
Larger rewrites are streamed to a different destination with explicit waits.

CRC detects damage, not a malicious publisher. App IDs are self-declared and
unauthenticated: different packages with the same ID share data. Install trusted
packages only until signed identity/install policy exists. A card can still
corrupt FAT metadata on power loss; use backups for important documents.
SD removal is polled every three seconds with a real sector read; a swap that
escapes polling is not a strong volume-identity guarantee. See `AUDIT.md`.
