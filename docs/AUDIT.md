# Architecture audit

Audit date: 2026-08-28. Scope: current Stage 3 source, build configuration and
all project documentation.

## Findings

| Priority | Finding | Decision/status |
|---|---|---|
| P1 | Notes owned an `lv_keyboard` whose matrix remained invisible on hardware despite valid focus, maps, geometry, style and foreground tests. | Replaced: the custom-button-matrix `SystemKeyboard` passed its initial isolated on-board check and now serves Notes through an adapter. Lifecycle regression checks remain open. |
| P1 | `DesktopShell` is nearly 2000 lines and owns platform services, shared overlays, window management and all built-in apps. | Do not rewrite all at once. Extract system keyboard first, then dialogs/file picker and built-in apps behind stable interfaces. |
| P1 | Storage public methods did not consistently canonicalize paths. | Fixed in this audit: all public path operations now use the same bounded component canonicalizer. |
| P1 | Wallpaper replacement removed the previous OWP before the new rename was known to succeed. | Fixed in this audit with backup/restore replacement. Full power-loss recovery of remnants remains Stage 4/6 work. |
| P2 | Architecture documentation mixed implemented Stage 3 code with planned Stage 4 tasks and services. | Current/target boundaries and a project map were added; roadmap now has Stage 3.1. |
| P2 | `StorageService` and diagnostic `SdCardDriver` both use global Arduino `SD`. | Accepted only because shell and diagnostics are mutually exclusive boot modes. Never run them concurrently. |
| P2 | `WallpaperService` includes LVGL private decoder headers. | Pin LVGL 9.5.0; add a compatibility test before any LVGL upgrade. |
| P2 | Date/time survives runtime but cannot account for powered-off duration. | Documented limitation; future RTC/NTP source enters through `setUtc()`. |
| P2 | No automated visual/touch test exists. | Isolated Keyboard Test is implemented with geometry/layout/memory telemetry; retain physical stage checklists. |
| P3 | Localization strings are distributed across `DesktopShell`. | Defer catalog extraction until built-in applications are separated. |
| P2 | Starting Stage 4 with an interpreter before defining package trust boundaries would expose malformed SD data to the VM. | YAP1 format, streaming CRC/bounds validator and deterministic host packer are implemented before Lua execution is enabled. |

## Verified strengths

- Board pins are centralized and match the physically tested CYD mapping.
- Display uses bounded partial buffers and no full-screen framebuffer.
- Touch calibration is versioned, migrates the legacy namespace and is shared
  between first boot and Settings.
- Kernel event/log storage is fixed-size and memory monitoring tracks the
  largest free block as well as total heap.
- Notes title/body buffers account for three-byte Cyrillic UTF-8 at configured
  character limits.
- Screen saver releases its transient star array and image cache entry.
- Diagnostics remains available without the SD card or a computer.

## Refactoring rule

Avoid a broad rewrite before Stage 4. Each extraction must preserve a physical
acceptance test and reduce `DesktopShell` ownership. The order is:

1. system keyboard;
2. system dialogs and file picker;
3. Notes controller/view;
4. Settings controller/pages;
5. shell/application lifecycle boundary.
