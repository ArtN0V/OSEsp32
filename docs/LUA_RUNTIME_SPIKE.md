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
- Open only the constrained base, coroutine, table, string, math and UTF-8
  libraries. Do not expose `io`, `os`, `package`, `debug`, dynamic loading or
  precompiled bytecode.
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

The clean firmware build uses 943,289 bytes of flash (51.4% of the 1,835,008
byte application partition) and 111,108 bytes of static RAM (33.9%). The prior
build used 827,153 bytes of flash and 110,944 bytes of static RAM. Dynamic heap
values are intentionally left pending until read from the physical result
screen.

The current runtime executes only self-terminating source applications. It
streams the verified `LUAS` section in 256-byte chunks, allows 16–96 KiB of Lua
heap as requested by the manifest, applies a 200,000-instruction and 250 ms
limit, and calls `lua_close` before any result UI is created. The only OSEsp32
API is `osesp32.ui.label(text)`; it copies at most 96 bytes and exposes no LVGL,
SD or native pointer.
