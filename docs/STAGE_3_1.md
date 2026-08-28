# Roadmap Stage 3.1 — stabilization and system UI extraction

## Goal

Close the known hardware and ownership gaps in Stage 3 before adding a Lua VM,
application permissions or more concurrent lifecycle states. Stage 3.1 is a
gate, not a feature-expansion stage.

## Work packages

### 1. System keyboard

- Implement `docs/SYSTEM_KEYBOARD.md` using one custom `lv_buttonmatrix`.
- Add an isolated physical Keyboard Test before integrating Notes.
- Support English, Russian and shared symbols through one style and geometry.
- Replace Notes-owned keyboard fields and callbacks with a client adapter.
- Verify clean teardown for fullscreen and future exclusive modes.

Exit: all seven physical keyboard checks pass.

Current status: the reusable button-matrix service, LVGL textarea adapter and
isolated **System Info → Keyboard Test** are implemented and compile. Physical
visibility, touch and repetition checks remain open. Notes deliberately still
uses its previous keyboard until the isolated test passes.

### 2. Storage hardening

- Canonicalize every public service path component, reject traversal, control
  characters, Windows separators and overlong output.
- Keep non-overwriting rename separate from transactional replacement.
- Preserve the old destination as `.bak` until a completed temporary file is
  installed.
- Add recognized `.tmp`/`.bak` mount recovery before Stage 4 applications can
  replace arbitrary user documents.

Current status: canonicalization and backup/restore replacement are complete;
mount-time remnant recovery is deferred to the application-storage work.

### 3. Shell decomposition boundary

- Keep one UI task and one active foreground application.
- Extract shared overlays before extracting application pages.
- Define one owner for keyboard, dialogs and future Open/Save picker.
- Move service construction toward `OSEsp32App` incrementally; do not perform a
  risky all-at-once rewrite of `DesktopShell`.

Exit: Notes no longer creates/deletes a keyboard and the next system
application can request text input without copying layout code.

### 4. Documentation and observability

- Keep `PROJECT_MAP.md` aligned with actual source ownership.
- Label planned Stage 4 types and tasks as planned, not current.
- Log keyboard state, dimensions, key count and memory baselines in its test.
- Record physical results instead of inferring success from compilation.

## Acceptance checks

1. Full clean build remains below 50% of the application partition and below
   40% static RAM.
2. Storage rejects `..`, `.`, backslashes, control characters and overlong
   paths while accepting ordinary UTF-8 names containing two dots internally.
3. A failed optimized-wallpaper replacement leaves the previous complete OWP
   recoverable.
4. System keyboard passes the physical gate in `SYSTEM_KEYBOARD.md`.
5. Notes creates, edits, saves and reopens Russian and English text; dirty-exit
   behavior works with the keyboard visible and hidden.
6. Repeated Notes and Keyboard Test open/close cycles show no downward heap or
   largest-block trend.
7. Screen saver, calibration and 0/180 rotation still pass after overlay
   extraction.

## Exit criterion

Stage 4 starts only after the keyboard is physically visible and reusable,
shared overlay ownership is unambiguous, storage checks pass, documentation
matches current code and no regression appears in Stage 3 hardware checks.
