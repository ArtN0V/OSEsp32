# Lua runtime footprint spike

Status: candidate selection prepared; target-board measurements are pending.

Official Lua release information checked on 2026-08-29:

- Lua 5.5.1 is the current 5.5 release: <https://www.lua.org/versions.html>.
- Lua 5.4.9 is the final 5.4 release and receives no later 5.4 releases:
  <https://www.lua.org/news.html>.

OSEsp32 keeps **Lua 5.4.9** as the compatibility baseline because the existing
runtime design was written against 5.4 and the branch is now final. Lua 5.5.1
is the measured challenger because its more compact arrays may help the
no-PSRAM board. Neither version is frozen into production until the same
on-board workload is measured with the table below.

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
| Flash increase | pending | pending |
| Idle VM heap | pending | pending |
| Workload peak | pending | pending |
| Smallest largest-free-block | pending | pending |
| Heap after 100 closes | pending | pending |
| Hook/allocator failure returns cleanly | pending | pending |

The smaller reliable result wins; a small footprint advantage cannot override
failed teardown or quota enforcement. Until this table is filled on the target,
the firmware validates `.yap` packages but does not execute their `LUAS` bytes.
