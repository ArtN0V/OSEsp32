# Roadmap Stage 4 — sandboxed YAP runtime

Status: foundation in progress. The frozen YAP1 parser/validator, host packer,
Hello sample and package-information view exist. No Lua VM,
`AppStorageService` or application task exists yet. The final Stage 3.1
lifecycle checks remain a gate before executing third-party code.

## Goal

Launch one external `.yap` application safely from SD, give it predictable RAM
and CPU budgets, allow useful but permission-controlled file operations, and
return to a clean desktop without requiring a computer or reboot.

## Decisions to freeze before implementation

1. **Runtime spike**
   - Compare suitable trimmed Lua releases/ports using the same Hello World,
     allocator and coroutine workload; freeze an exact version in the build.
   - Record idle VM RAM, minimum heap, largest free block, flash cost and
     teardown baseline. A custom `lua_newstate` allocator is mandatory.
   - Keep only safe standard libraries. Do not expose `io`, `os`, `package` or
     native module loading directly.

2. **YAP1 manifest and package reader**
   - Define application ID, name, API version, entry point, launch-mode request,
     memory request, capabilities and file-type associations.
   - Validate bounds, integer overflow, duplicate sections and CRC before Lua
     sees package bytes. Keep the container uncompressed and seekable so code
     and resources can be loaded on demand.
   - Treat file associations as candidates only. The shell resolves conflicts
     through Open with and never lets package installation silently replace a
     user default.

3. **Lifecycle state machine**
   - Implement `Preparing → Running → Stopping → RestoringShell`, with failure
     paths that always reach the built-in shell.
   - Support `windowed`, `fullscreen` and `exclusive`. The OS chooses the final
     mode from manifest request, memory pressure and policy.
   - Before exclusive launch, close shell files, save compact navigation state,
     destroy desktop objects, clear wallpaper/image caches and verify both free
     heap and largest contiguous block.
   - Keep a system-owned exit gesture/overlay and watchdog path available even
     when the application occupies the full screen.
   - On stop, unregister callbacks, close handles, delete the app screen,
     `lua_close` the whole VM, clear caches, compare memory to baseline and
     rebuild the shell.

4. **Resource governance**
   - Route every Lua allocation through a hard quota allocator and report a
     controlled out-of-memory error.
   - Use instruction-count hooks and cooperative callbacks to prevent one app
     from starving touch, display and storage processing.
   - Bound callback duration, queued events, timers, open files, decoded assets
     and native canvas memory independently from the Lua heap.

## Application filesystem API v1

### Namespaces

- `app:/path` reads a resource from the active package and is always read-only.
- `data:/path` maps only to `/OSEsp32/Data/<app-id>/` and supports private
  application state, autosave and temporary files.
- User files are represented by opaque handles returned by system Open/Save
  dialogs. Apps never construct or receive an unrestricted SD path.

### Manifest capabilities

Initial capabilities are intentionally narrow:

- `storage.private.read`
- `storage.private.write`
- `documents.open` with declared extensions/MIME classes
- `documents.create` with declared extensions/MIME classes
- `documents.replace`, requested separately because it can overwrite a file

The manifest is only a request. The shell confirms user-document access through
the picker and binds permission to one selected file and one app session.

### Operations and limits

- Provide bounded read/write/seek/size/flush/close/stat and paged private
  directory listing.
- Define create-new, truncate, append and transactional-replace semantics
  explicitly; do not route application writes through LVGL's asset filesystem
  callbacks.
- Canonicalize every path and reject `..`, absolute paths, invalid UTF-8,
  control characters and overlong components.
- Limit each application to a small fixed handle table (initial target: four),
  cap each transfer and yield between chunks. Reserve free space so an app
  cannot fill the card completely.
- Invalidate all handles on removal. Return stable error codes instead of Lua
  crashes or ambiguous empty data.
- Never expose raw `SD`, Arduino `File`, LVGL `S:` paths, pointers or native
  descriptors.

### Transactional save

Document replacement is a system operation:

1. create a uniquely owned sibling `.tmp` file;
2. stream data, verify byte counts, flush and close;
3. move an existing destination to a recognized `.bak` name;
4. rename the completed temporary file to the destination;
5. remove the backup only after success;
6. repair or offer recovery for known remnants on the next mount.

This does not make FAT fully power-fail-safe, but avoids deliberately deleting
the only good copy before the replacement is complete.

## Explicitly paged data, not swap

Stage 4 streams package code and resources. A future `PagedBlob`/`PagedArray`
may keep a small fixed cache of file pages for large application datasets. Such
objects expose indexed methods and can fail with `storage_removed`; they never
pretend that SD bytes are normal Lua or C++ RAM. A generic `swap.bin` is not
part of OSEsp32.

## Implementation order

1. Confirm Stage 3.1 system keyboard and shared-overlay lifecycle gates.
2. Runtime/allocator footprint spike and version freeze. **Candidate plan
   complete; target measurements pending.**
3. YAP1 parser, validator and manifest capability model. **Implemented; touch
   UI and malformed-package hardware checks pending.**
4. Minimal Hello World in windowed mode.
5. Lifecycle manager and exclusive shell teardown/rebuild.
6. Instruction/time enforcement and system exit path.
7. `app:/` and `data:/` storage with fixed handle table.
8. Shell-owned Open/Save dialogs and exact-file capabilities.
9. Transactional save/recovery and SD-removal pause flow.
10. File round-trip sample, packaging tool skeleton and endurance tests.
11. File-association registry with Open with conflict resolution and persisted
    user defaults.

## Acceptance checks

1. Reject malformed/truncated YAP packages without starting Lua.
2. Demonstrate that a VM cannot exceed its allocator quota or block the shell
   indefinitely; both cases return to the desktop with an explanation.
3. Launch/close windowed and exclusive Hello World 100 times. Free heap and
   largest block must stabilize rather than trend downward.
4. In exclusive mode, measure and record RAM before shell teardown, before VM
   creation, at application peak and after shell restoration.
5. Confirm an app can read its package, write private data and cannot escape
   `data:/` using traversal, absolute paths or malformed UTF-8.
6. Confirm a document app cannot open or replace a user file until the system
   picker grants that exact file.
7. Save a file over an existing document, interrupt each transaction phase in
   tests, then confirm either the old or new complete copy can be recovered.
8. Remove SD during read, write and exclusive execution. The system must keep
   display/touch/exit control, invalidate handles and offer Retry/Close without
   crashing or silently losing the prior document.
9. Reinsert a different card and verify old handles do not become valid merely
   because the same path exists.
10. Register two BMP handlers and confirm the shell offers Open with, honors a
    chosen default and allows resetting it without changing either package.

## Exit criterion

All checks pass on the no-PSRAM ESP32-2432S028. The runtime has measured limits,
exclusive launch reliably restores the shell, no application has ambient SD
access, and the storage contract is sufficient to implement Paint in Stage 5.
