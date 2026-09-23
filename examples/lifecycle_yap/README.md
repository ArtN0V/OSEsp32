# Lifecycle demos

Run `python3 tools/build_yap_examples.py` (Windows: `py -3 tools\build_yap_examples.py`).
Copy `build/lifecycle_windowed.yap`, `build/lifecycle_fullscreen.yap` and
`build/lifecycle_exclusive.yap` to `/OSEsp32/Apps` on SD.

Each demo displays an increasing counter until the OS-owned EXIT button is
pressed. `osesp32.sleep(1000)` pauses Lua without blocking touch, rendering or
storage probes. Windowed retains the desktop; fullscreen covers it; exclusive
destroys desktop objects and releases the wallpaper strips. Only one app is
active and the underlying shell does not accept input during the session.

Test with wallpaper enabled and after using Notes (to allocate the keyboard):

1. Run each mode, wait 5 seconds, verify the counter and press EXIT.
2. Expect `cancelled`, `After close: 0 B`, restored wallpaper/taskbar and working
   Notes keyboard. The result page scrolls to memory metrics and RUN AGAIN.
3. Repeat exclusive start/exit 100 times. Compare steady-state runs with the
   same wallpaper/keyboard state; lazy allocations make cold/warm runs differ.
4. Test at both screen rotations and both interface languages.
5. Remove SD during the counter demo: once storage detects removal, the app
   closes with `io_error` and the desktop remains usable without wallpaper.
6. Run the existing failure examples with their manifest mode changed to
   `exclusive`: every failure must restore the shell, with zero Lua bytes left.

Physical SD detection depends on the existing StorageService probe; it is not
instantaneous. These checks are pending on the CYD; desktop runtime tests do
not establish touch/display or ESP32 heap behavior.
