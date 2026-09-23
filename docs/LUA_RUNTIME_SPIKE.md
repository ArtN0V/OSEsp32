# Lua runtime footprint spike

Status: Lua 5.4.9 baseline frozen and compiled; target-board dynamic
measurements are pending.

Official Lua release information checked on 2026-08-29:

- Lua 5.5.1 is the current 5.5 release: <https://www.lua.org/versions.html>.
- Lua 5.4.9 is the final 5.4 release and receives no later 5.4 releases:
  <https://www.lua.org/news.html>.

OSEsp32 freezes **Lua 5.4.9** for the first runtime because the runtime design
targets 5.4 and that release line is final. It is installed from the official
archive by `tools/install_lua.py`, verified by SHA-256, trimmed to the selected
libraries and configured for 32-bit integers/floats. Lua 5.5.1 remains a future
challenger only if on-board measurements show that 5.4.9 cannot meet the
no-PSRAM budget; package API v1 does not expose Lua-version-specific behavior.

## Mandatory build configuration

- Build Lua from official source; exclude the standalone `lua` and `luac`
  programs.
- Use `lua_newstate` with an OSEsp32 allocator that stores current/peak bytes
  and refuses allocations above the manifest quota.
- Open only the constrained base, table, string, math and UTF-8
  libraries. Do not expose `io`, `os`, `package`, `debug`, dynamic loading or
  precompiled bytecode.
- Coroutine creation/resume belongs to the host; the Lua coroutine library
  is not exposed. Metatable mutation, table.sort and string pattern operations
  are also unavailable pending bounded implementations.
- Install an instruction hook before application code runs.
- Run source from a validated `LUAS` package section and call the manifest's
  entry function.
- Close the complete state after every run and compare total heap and largest
  free block against the pre-launch baseline.

## Identical workload

1. Create an empty state and open the allowed libraries.
2. Load/call Hello World from `examples/hello_yap`.
3. Allocate and release strings, maps and arrays up to a 32 KiB quota.
4. Create/resume/close one coroutine 100 times.
5. Trigger allocator rejection and instruction-hook termination deliberately.
6. Destroy the state and repeat the complete run 100 times.

## Hardware result table

| Metric | Lua 5.4.9 | Lua 5.5.1 |
|---|---:|---:|
| Flash increase | 116,136 B (clean PlatformIO build) | not measured |
| Idle VM heap | pending | pending |
| Workload peak | pending | pending |
| Smallest largest-free-block | pending | pending |
| Heap after 100 closes | pending | pending |
| Hook/allocator failure returns cleanly | pending | pending |

The first execution-slice build used 943,289 bytes of flash (51.4% of the 1,835,008
byte application partition) and 111,108 bytes of static RAM (33.9%). The prior
build used 827,153 bytes of flash and 110,944 bytes of static RAM. Dynamic heap
values are intentionally left pending until read from the physical result
screen.

The lifecycle build (2026-09-18) uses 944,745 flash bytes and 86,740 static RAM
bytes. Wallpaper strips moved from permanent storage to a lazy allocation,
so a desktop with wallpaper still needs about 25 KiB of dynamic cache. This
cache is released for exclusive execution.

The current runtime streams verified source in 256-byte chunks, runs one
host-owned coroutine and yields at 1,000-instruction intervals. A burst between
explicit waits is limited to 200,000 instructions / 250 ms active execution;
these are not a wall-clock deadline for compilation or native C routines.
`osesp32.sleep(1..60000)` allows long-lived apps and resets the burst budget on
wake. `osesp32.ui.label(text)` copies at most 96 bytes. Neither API exposes
LVGL, SD or native pointers. EXIT and detected SD removal close the entire VM.

`python3 tools/test_yap_runtime.py` passed real-runtime host tests with
ASan/UBSan: 100 Hello runs, quota rejection, a loop inside pcall, syntax error,
missing entry, timed sleep including clock wrap, cancellation, simulated SD
removal/short reads and lifecycle transitions. Actual
ESP32 heap and screen restoration still require the lifecycle hardware guide.
