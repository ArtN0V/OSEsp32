# Lifecycle demos

Run `python3 tools/build_yap_examples.py` (Windows: `py -3 tools\build_yap_examples.py`).
Copy `build/lifecycle_windowed.yap`, `build/lifecycle_fullscreen.yap` and
`build/lifecycle_exclusive.yap` to `/OSEsp32/Apps` on SD.

Each demo counts down ten seconds then calls `osesp32.exit()`, returning directly
to the desktop. The same call can be used by a future game's menu handler;
the current minimal UI API does not yet expose button callbacks.
`osesp32.sleep(1000)` pauses Lua without blocking touch, rendering or
storage probes. Windowed retains the desktop; fullscreen covers it; exclusive
destroys desktop objects and releases the wallpaper strips. Only one app is
active and the underlying shell does not accept input during the session.

Test with wallpaper enabled and after using Notes (to allocate the keyboard):

1. Run each mode and wait ten seconds. Expect the desktop, restored wallpaper
   and working Notes keyboard; no diagnostic report opens on app exit.
2. Run again and stop early: windowed has EXIT; fullscreen/exclusive have no
   system title bar or exit button. Hold the top-left 32x32 pixels for two seconds.
   Expect `cancelled`, `After close: 0 B`. The report scrolls to RUN AGAIN.
   Short taps, leaving the corner and the launch touch must not trigger exit.
3. Repeat exclusive start/exit 100 times. Compare steady-state runs with the
   same wallpaper/keyboard state; lazy allocations make cold/warm runs differ.
4. Test at both screen rotations and both interface languages.
5. Remove SD during the counter demo: once storage detects removal, the app
   closes with `io_error` and the desktop remains usable without wallpaper.
6. Run the existing failure examples with their manifest mode changed to
   `exclusive`: every failure must restore the shell, with zero Lua bytes left.

`osesp32.exit()` is terminal, including inside `pcall`: statements after the
call do not execute. Save data and ask any confirmation before calling it.

Physical SD detection depends on the existing StorageService probe; it is not
instantaneous. These checks are pending on the CYD; desktop runtime tests do
not establish touch/display or ESP32 heap behavior.
