# Project audit

Audit date: 2026-09-24. Scope: board/kernel/services/UI/shell/runtime sources,
Arduino/PlatformIO configuration, packaging tools, examples, automated tests and
all architecture/roadmap documentation. This audit distinguishes host/build
evidence from behavior that still needs the ESP32-2432S028.

## Outcome

Stage 4 is code-complete. No known P0/P1 source defect remains after the fixes
below. Stage 5 work packages 1–3 are accepted on the board, and Work package 4
has started without waiving the remaining
Stage 4 gates. Stage 4 is not hardware-accepted: display/touch, real FAT interruption,
removal/reinsertion and heap-fragmentation checks cannot be proved by a desktop
compiler or mocked filesystem. New feature evidence must not be mistaken for
completion of the separate Stage 4 checklist.

Current reproducible build after the first API 1.4 Paint slice:

- static RAM: 101,220 bytes / 327,680 (30.9%);
- flash: 985,865 bytes / 1,835,008 (53.7%);
- PSRAM: not used or assumed;
- LVGL: two 320x20 RGB565 partial buffers; no full-screen framebuffer;
- Lua: 16–96 KiB quota, one VM, one host coroutine;
- native YAP bounds: four files, 512-byte operations, 24 API 1.2 widgets,
  12 aggregate list rows, four timers and eight events.
- Canvas probe: one exclusive 320x204 native buffer in RGB565/I8/I4, never a
  Lua pixel table or second full-size framebuffer.
- Paint slice: one variable-size exclusive I4 Canvas, bounded clear/line
  commands and coordinate touch events.

These are linker figures, not live heap measurements. Rebuild figures may move
slightly with toolchain/library versions; LovyanGFX is now pinned at 1.2.28 and
LVGL at 9.5.0 for reproducibility.

## Fixed in this audit

| Priority | Finding | Resolution |
|---|---|---|
| P1 | YAP had lifecycle execution but no safe useful files/UI contract. | Added `AppStorageService`, `YapUiHost`, system keyboard requests, exact-file Open/Save, queued buttons, private/resources API and two examples. |
| P1 | Document replacement/removal could revive stale authority or lose the only known old copy. | Added monotonic session tokens, SD generations, invalidation, YTX1 checksummed journals, staged data/backup recovery and Retry revalidation. Damaged remnants are preserved. |
| P1 | SD presence polling used cached card metadata and might miss removal. | Periodic check now performs a physical sector read and increments a generation on removal and mount. |
| P1 | Runtime could accept a newer unsupported API minor or truncated parser reads/paths. | Minor version, exact read lengths and pre-copy path size are validated; packer/inspector parity tests were expanded. |
| P1 | Package resources had no frozen name encoding or duplicate/path validation. | RSRC now uses a 64-byte UTF-8 relative-name header; firmware and packer reject traversal, malformed names and case-insensitive duplicates. |
| P1 | App/user paths could be malformed UTF-8, escape namespaces or be silently truncated by directory listing. | Strict component validation was added; overlong SD entries are omitted instead of published under a different truncated path. |
| P1 | Lua response allocation could happen outside a protected VM resume. | Text/error results are now pushed by the Lua continuation inside `lua_resume`. |
| P1 | Clock elapsed time wrapped after about 49.7 days; invalid calendar dates and negative-zone arithmetic were unsafe. | Uses 64-bit ESP timer, validates dates/timezones/range and performs signed checked local-time conversion; host rollover/calendar tests added. |
| P2 | Built-in Notes/OWP replacement preserved `.bak` but had no narrowly scoped consumer recovery. | Only recognized Notes files and the fixed desktop OWP destination can restore/remove their own backups before access. |
| P2 | Image Viewer was hard-wired and third-party handlers had no conflict/default policy. | Added bounded one-package-per-loop registry, Open with, explicit remembered default and Settings reset. |
| P2 | Heap allocation sites used throwing `new` despite embedded no-exception expectations. | Notes, wallpaper decoder, screen saver and SD LVGL adapters use `std::nothrow` and existing failure paths. |
| P2 | Generated Python bytecode was tracked in Git. | Removed the recoverable generated cache and ignored `__pycache__/`/`*.py[cod]`. |
| P2 | Documentation described implemented work as future and omitted failure semantics. | Added `YAP_API.md`; updated roadmap, architecture, project map, YAP1, Stage 4 and README. |
| P2 | Application UI was limited to six fixed buttons and one output label. | Added versioned API 1.2 host-owned widgets, fixed geometry/count/text limits, queued rich events, confirmations/timers and a reference Calculator without exposing LVGL. |
| P1 | UI preparation queried the runtime before the new package had started, so it could use the previous app's API version. API 1.2 first opened blank and then poisoned the next API 1.1 launch after emergency exit. | `DesktopShell` now passes the inspected package's API/layout explicitly into `YapUiHost`; the selection is immutable for that foreground session. |
| P2 | Paint's Canvas format had only estimates, so committing an RGB565 or indexed contract could exhaust or fragment a no-PSRAM board. | Target measurements reject RGB565 (`out_of_memory`) and I8 (20,468-byte largest block) and select I4 (144,500 bytes free, 49,140-byte largest block). |
| P1 | Repeating I8/I4 first closed on cycle two or three; after replacing `lv_canvas`, a later run still appeared to reboot at random probe operations. | `YapUiHost` uses a plain image, detaches its source before ordered object/buffer destruction, and performs no cache calls because caches are disabled. A real 24 KiB Lua VM passes 64 continuations, RTC reset breadcrumbs remain available, and the subsequent target retest was reported stable. |

## Remaining risks and debt

| Priority | Risk | Required action |
|---|---|---|
| P1 gate | No complete physical Stage 4 run exists for the current build. Host mocks cannot validate LVGL stacking, resistive-touch behavior, real SD controller failures or actual heap fragmentation. | Run every check in `STAGE_4.md`, including wallpaper/keyboard/exclusive cycles and live heap/largest-block baselines. |
| P1 gate | A single successful I4 run does not establish long-term restoration or fragmentation stability. | Repeat ten I4 launch/release/exit cycles, record the released baseline and then run Calculator and `file_roundtrip.yap`. |
| P1 gate | FAT rename/flush is not power-fail atomic. Journals select an old or newly installed complete logical file, but card firmware/FAT metadata can still corrupt either. | Use disposable files for fault tests; verify power cuts at every phase on real media and retain external backups. |
| P2 | Card generation proves an observed unmount/remount, not cryptographic volume identity. A rapid swap between 3-second probes or indistinguishable cloned card cannot be strongly identified. | Later store/compare a volume fingerprint or CID where the core exposes it; never shorten the current package revalidation on Retry. |
| P2 | YAP CRC is corruption detection, not publisher authentication. IDs are self-declared; packages sharing an ID share private data. | Stage 6 installer/signature/identity policy. Until then install trusted packages only. |
| P2 | A single package validation/CRC scan (up to 8 MiB) is synchronous. It yields to RTOS housekeeping every 4 KiB but not to LVGL; Open with can briefly feel frozen. | Add an incremental validator/state machine before large repositories or packages are common. |
| P2 | `DesktopShell.cpp` is still 2,509 lines and owns built-in apps/window routing. `YapUiHost` is extracted, but Notes/Settings/dialogs are not. | Continue controller-by-controller extraction after Stage 4 acceptance; avoid a broad rewrite. |
| P2 | No automated LVGL/touch test harness exists. Allocation failures inside many legacy shell widget constructors rely on LVGL's configured malloc assertion. | Add simulator/UI event tests and explicit low-memory admission/fallback paths in Stage 6. |
| P2 | File limit is per file, not aggregate per application; many 1 MiB files can consume a card while keeping the 128 KiB reserve. | Add an aggregate private-data quota/usage index in Stage 6. |
| P2 | Association registry intentionally scans only 64 root app entries and retains eight matches. Default paths can become stale after manual SD file moves. | Add an installer-maintained registry in Stage 5/6; current UI exposes truncation and ignores stale defaults safely. |
| P3 | Settings/localized strings and legacy confirmation dialogs remain distributed through `DesktopShell`. | Extract a catalog and common dialog owner as built-in apps are separated. |
| P3 | `WallpaperService` depends on LVGL private decoder/cache headers. | Keep LVGL 9.5.0 pinned; require a dedicated compatibility test before upgrades. |
| P3 | Software time cannot know powered-off duration without RTC/network. | Future NTP source calls `DateTimeService::setUtc`; do not imply persisted checkpoint is an RTC. |
| P3 | Recovery can change a Notes directory between paged listings, so one card may be skipped until the gallery is reopened. It cannot duplicate or corrupt a note. | Accept for rare boot recovery; a later Notes index can perform a dedicated recovery pass before paging. |

## Security and ownership checks

- GPIO numbers occur only in `BoardConfig.h`; display/touch/SD drivers use it.
- Shell-mode SD calls are confined to `StorageService`; diagnostic `SdCardDriver`
  is allowed only because diagnostic and shell boot modes are mutually exclusive.
- Applications receive no raw SD paths from pickers, native handles/pointers,
  LVGL objects, GPIO, network, dynamic native loading or bytecode.
- Private paths map below `/OSEsp32/Data/<app-id>` and document grants exclude the
  whole `/OSEsp32` tree. Create and replace are separate manifest permissions.
- All system overlays mutate LVGL from the Arduino/UI loop. LVGL callbacks queue
  actions; they do not resume Lua or delete their own active targets.
- Fullscreen/exclusive show no system exit button. Normal apps call `exit()`;
  the calibrated top-left two-second recovery hold remains OS-owned.
- Runtime allocator accounting reaches zero after teardown in host endurance;
  real ESP32 allocator/fragmentation remains a hardware gate.

## Verification evidence

`tools/test_yap_runtime.py` compiles the actual runtime, package parser,
application storage, clock and vendored Lua with ASan/UBSan. It covers malformed
packages with repaired CRCs, 100 launches + 100 exits, quota/pcall limits,
UTF-8/traversal, package resource bounds, exact capabilities, handle exhaustion,
stale handles after a different generation, interrupted transaction phases,
text/button/document requests, API 1.2 widget bounds/events/confirmation,
API 1.3 Canvas request/release continuations, API 1.4 drawing/touch requests and
the real packed round-trip demo.

Python tests cover deterministic pack/inspect, resource paths, supported API,
mandatory/overlapping/unknown sections and every example package. PlatformIO
compiles the Arduino-ESP32/LVGL/LovyanGFX target. Passing these means source/API
consistency; it does not mark the hardware checklist passed.

## Stage decision

Stage 4 implementation gate: **passed**. Its full hardware acceptance gate is
still **open**. Stage 5 Work packages 1–3 are accepted on the board. Work
package 4 now has the first host/build-tested Paint drawing slice; BMP
round-trip and SD recovery remain. Any failed Stage 4 check continues to take
priority.
