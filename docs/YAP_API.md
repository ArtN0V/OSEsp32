# OSEsp32 YAP API 1.1

API 1.0 packages remain valid. New apps should declare `"api_minor": 1`.
Lua 5.4.9 uses 32-bit integers/floats and one host-owned coroutine. All APIs
below are under `osesp32`. No native pointers, LVGL objects, paths from user
pickers, io/os/debug/package or Lua-created coroutines are exposed.

## Execution and UI

| Call | Contract |
|---|---|
| `ui.label(text)` | Replace output, valid UTF-8, at most 96 bytes. |
| `ui.button(id, text)` | Show/update one of six buttons (1..6); empty text hides it; 48-byte caption. |
| `ui.wait()` | Yield until a button is tapped; returns its integer ID. Eight queued IDs, excess taps dropped. |
| `ui.text(initial)` | System keyboard dialog; initial <=96 bytes, result <=64 characters/192 bytes; cancel returns nil, `cancelled`. |
| `sleep(ms)` | Yield for 1..60000 ms, reset active-execution budget after wake. |
| `exit()` | End the application immediately; never returns, even inside pcall. Save first. |

There are no Lua callbacks inside LVGL. Shell-owned widgets never escape into
the VM. Text/event/document waits reset the burst budget only after an external
response; instruction-hook yields alone do not reset it. Text strings reject
embedded NUL and malformed UTF-8; binary file data does not.

Fullscreen/exclusive have no system title bar or EXIT button. Apps provide
their own button calling exit(). An invisible 2-second top-left hold remains
for emergency cancellation. Windowed apps also retain the system EXIT button.

## Files

Operations return a result (or true), or `nil, error`. Argument type/range
errors raise ordinary Lua errors. Handles are positive session tokens, not
native descriptors. Never assume a particular token or reuse a closed one.

| Call | Contract |
|---|---|
| `fs.open(path, mode)` | `app:/name` read-only resource; `data:/name` private file. Modes r/w/x/a. |
| `fs.read(h, n)` | Binary string, 0..512 bytes; empty string at EOF; short I/O is an error. |
| `fs.write(h, bytes)` | At most 512 bytes per call; staged transaction only. |
| `fs.seek(h, offset)` | Absolute offset 0..size, no sparse holes. |
| `fs.size(h)` | Byte length. |
| `fs.flush(h)` | Validate live handle; writes are already flushed/closed per chunk. Does not commit. |
| `fs.close(h [, commit])` | Default true commits; false discards staged changes. Closed token never revives. |
| `fs.stat(path)` | Readable file size (not directory metadata). |
| `fs.list("data:/folder", page)` | Zero-based page; up to four `{name,directory,size}` entries, no raw SD paths. |
| `fs.mkdir("data:/folder")` | Create directory; parents must exist. |

`r` reads; `w` stages a new/truncated private file; `x` refuses existing files;
`a` copies existing content and starts at EOF. Append is limited to a 4 KiB
existing file to bound synchronous copy time; use streamed rewrites for larger
documents. At most four handles, 1 MiB writable file, 128 KiB remaining card
reserve. There is not yet an aggregate quota on all files belonging to one app.
Paths are at most 127 bytes as Lua arguments, components at most 48 UTF-8 bytes,
and resolved native paths must fit 128 bytes. Traversal, absolute paths, invalid
UTF-8, empty components, reserved FAT characters and trailing spaces/dots fail.

Private read and write require separate manifest capabilities. Append requires
both. Resources are always readable without private permission. A write handle
can read its own staged bytes, not the old destination. There is no delete or
rename API in 1.1. Abnormal exit and ordinary exit discard uncommitted writes.
Open handles must be explicitly closed to commit. Larger transfer loops should
call sleep(1) between 512-byte chunks; native SD calls cannot be preempted.

## User documents

- `documents.open()` displays the system folder/file picker and returns one
  read-only handle. Requires documents.open and a declared extension.
- `documents.save("name.txt")` displays Save As. Create new uses
  documents.create, refusing collisions. Selecting an existing file prompts
  for replacement and requires documents.replace separately. Apps cannot
  silently acquire replacement permission by asking to create.
- `documents.current()` consumes the initial read handle when launched through
  Files → Open with; otherwise returns nil, no_document. The handle itself
  remains usable until closed or invalidated.

The picker starts in `/Documents`, supports folders, parent navigation and
paging. `/OSEsp32` is excluded. Save's proposed name is supplied by the app;
use ui.text before save if the app needs filename editing. Cancel returns
nil,cancelled. A manifest alone never grants a user document.

## Recovery and stable errors

Common errors: permission_denied, invalid_path, invalid_mode, invalid_handle,
not_found, already_exists, too_many_handles, transfer_too_large, invalid_offset,
quota_exceeded, append_too_large, no_space, busy, storage_removed, io_error,
recovery_required, bad_resource, cancelled, no_document.

Detected SD removal pauses the VM and invalidates every handle. Retry verifies
the package again, repairs known transactions and resumes RAM state. Apps must
reopen private/resources or show another picker; old document grants are gone.
A pending text/document request receives storage_removed after Retry. A waiting
button request keeps waiting. The app decides whether/how to retry its logical
operation. Close discards the VM. No fallback to internal flash occurs.

Transactions use four slots in `/OSEsp32/Transactions`: `.txn` is ASCII
`YTX1:<8-hex IEEE CRC32 of target>:<absolute target>`, `.data` staged bytes,
`.old` old destination. Journal is flushed before staging. Commit renames old
destination to backup, staged file to destination, then removes backup/journal.
Recovery without installed target restores backup; with target it keeps the
new complete file. Partial/unrecognized journals are preserved for manual
recovery and block app launch. Do not delete remnants blindly; back up the card.
This is logical recovery, not a guarantee against FAT metadata corruption.

## Package resources and associations

Manifest JSON may contain `"resources": {"welcome.txt": "welcome.txt"}`.
Source paths are relative to the manifest; names are relative app:/ paths.
At most 14 resources with two mandatory sections. Each RSRC is a 64-byte
NUL-terminated UTF-8 name followed by raw bytes (including zero-length data).
Duplicate names ignoring ASCII case are rejected. Names resolve case-sensitively.

Only root `/OSEsp32/Apps` entries are registered: first 64 entries, up to eight
matching candidates including Viewer. Limits are shown, not unbounded growth.
Invalid packages are ignored. Scanning validates one package per UI loop; an
individual large CRC scan can still pause touch. Defaults are explicit NVS
choices and are reset via Settings → Default apps. CRC/app ID is not a signature;
packages claiming the same ID share private data. Install trusted apps only.

Examples: `examples/file_roundtrip_yap`, `examples/document_info_yap`.
Stage 5 adds Canvas/Paint and richer SDK, not general SD-backed RAM.
