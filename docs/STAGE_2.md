# Roadmap Stage 2 — graphical shell

## Goal

Replace the foreground diagnostic screen with a small Windows-inspired LVGL
shell while preserving hardware diagnostics as a recovery mode. The shell must
remain responsive on the classic ESP32-2432S028 without PSRAM.

## Implementation steps

### 1. LVGL configuration and memory boundary

- Pin LVGL 9.5.0 for reproducible Arduino IDE and PlatformIO builds.
- Use RGB565 and two internal-RAM `320 x 20` partial draw buffers.
- Give LVGL objects a 64 KiB budget through the monitored ESP32 heap; avoid a
  static arena that can overflow the classic ESP32 DRAM linker segment.
- Do not allocate a full-screen framebuffer.

Acceptance: the firmware builds below both partition and RAM limits, and the
kernel monitor remains above its 64 KiB emergency reserve.

### 2. Display and pointer port

- `LvglPort` owns the single display and touch driver used by the shell.
- A flush callback sends only invalidated rectangles through LovyanGFX.
- A pointer callback supplies the already calibrated XPT2046 coordinates.
- LVGL ticks and timers run from the Arduino main loop; UI calls stay on that
  task.

Acceptance: buttons track the stylus, colors are correct and partial redraws do
not leave artifacts.

### 3. Shell and window manager

- Create a desktop, bottom taskbar, Start button, uptime clock and desktop
  shortcuts.
- Keep one foreground window on the 320 x 240 display.
- Provide a title bar, close action and a stable content area.
- Opening a new app replaces the existing foreground window rather than
  retaining hidden object trees.

Acceptance: Start and every desktop shortcut can repeatedly open and close
without reducing free heap steadily.

### 4. Built-in applications

- About reports version and platform.
- Settings persists backlight brightness and launches touch calibration.
- System Info reports heap, events, faults and calibration state.
- Text Input demonstrates the on-screen keyboard.
- Files is a Stage 3 placeholder rather than an incomplete filesystem browser.

Acceptance: every built-in window is reachable by touch and has a working
close action.

### 5. Calibration integration

- If no NVS calibration exists, show calibration before normal desktop use.
- Settings can start the same `TouchCalibrationService` at any time.
- While calibrating, LVGL pointer input is suspended so a press cannot activate
  controls behind the overlay.
- Failed fits keep the previous runtime calibration.

Acceptance: five points save successfully, reboot restores NVS values and the
desktop corners align with the stylus.

### 6. Recovery diagnostics

- Store a one-shot boot request in NVS and restart into the proven Stage 0/1
  hardware diagnostics.
- Diagnostics provides an on-screen Shell button to return by restarting;
  Serial remains optional.
- A failed SD card is irrelevant to shell boot.

Acceptance: System Info can enter diagnostics, all old tests remain available,
and returning to the shell requires neither reflashing nor a computer.

## Physical verification

1. Boot and check desktop, taskbar, Start menu and clock.
2. Open and close each shortcut ten times; compare `k` output in diagnostics.
3. Test the keyboard across the full screen width.
4. Recalibrate from Settings and reboot.
5. Enter hardware diagnostics, run all tests and return to the shell.
6. Leave the desktop running for 30 minutes and confirm no reset or visible
   corruption occurs.
