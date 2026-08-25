# Roadmap Stage 1 — platform foundation

## Goal

Introduce small, deterministic system services underneath the proven Stage 0
drivers. The diagnostic interface remains the active foreground application,
which lets every new service be exercised before LVGL and the desktop arrive.

## Implementation steps

### 1. Project identity and entry point

- Project directory and Arduino sketch are both named `OSEsp32`.
- `OSEsp32App` owns the system kernel and current foreground application.
- Arduino IDE and optional PlatformIO builds use the same application object.

Acceptance: opening `OSEsp32.ino` in Arduino IDE recognizes the entire folder
as one sketch and produces exactly one `setup()` and `loop()`.

### 2. Fixed-memory logging

- Every entry has timestamp, severity, short tag and bounded message.
- The last 24 entries stay in a static ring buffer for a future on-screen log.
- Serial remains the live output transport during development.
- No Arduino `String` allocations are used by the logger.

Acceptance: boot creates kernel and memory log entries; command `k` reports a
non-zero log count, and repeated operation does not reduce free heap steadily.

### 3. Static system event bus

- A FreeRTOS static queue holds 16 typed events.
- Publishers never block the UI.
- Queue overflow is counted and converted into a visible system fault.
- Stage 1 kernel is the dispatcher; Stage 2 will add shell subscribers.

Acceptance: calibration and memory monitoring generate handled events, while
the dropped-event counter remains zero during normal use.

### 4. Lifecycle and fault reporting

- States: booting, diagnostics, running, safe mode and fault.
- Faults retain a bounded last-fault record and cumulative session count.
- Reset reason is logged at boot.
- Low memory, event overflow and rejected calibration have explicit codes.

Acceptance: command `k` reports lifecycle `diagnostics`, zero faults after a
normal boot and the previous reset reason in the Serial log.

### 5. Memory monitoring and budget guardrails

- Heap, minimum heap, largest contiguous block and free PSRAM are sampled every
  five seconds.
- Initial warning thresholds are 64 KiB free heap and 32 KiB largest block.
- Crossing a threshold emits one event/fault rather than flooding the queue.

Initial no-PSRAM budget target:

| Consumer | Planned ceiling |
|---|---:|
| kernel, queues and logs | 8 KiB |
| LVGL objects and theme | 64 KiB |
| two 320x20 RGB565 draw buffers | 25 KiB |
| active application VM | 64 KiB |
| filesystem and transient I/O | 32 KiB |
| emergency free-heap reserve | at least 64 KiB |

These are guardrails, not measurements. They will be revised from command `k`
and stress-test output on the physical board.

### 6. Touch calibration as an OS service

`TouchCalibrationService` owns sample collection and linear regression.
`TouchDriver` owns raw reads, coordinate transformation and versioned NVS
storage. The diagnostic screen only renders prompts and progress.

Integration into the final OS:

1. **Stage 1:** reusable service, event reporting and NVS persistence.
2. **Stage 2:** first-boot setup invokes it when no stored calibration exists.
3. **Stage 2:** Settings exposes “Touch calibration” and reset actions.
4. **Stage 4:** third-party applications receive calibrated coordinates only;
   raw readings and calibration modification stay privileged.

Acceptance: calibration survives reboot, diagnostics and the future shell call
the same service, and invalid calibration preserves the previous good values.

## Stage 1 verification

1. Upload and confirm display/touch diagnostic menu still works.
2. Send `k` immediately and again after at least 15 seconds.
3. Run touch calibration; reboot and confirm boot log says `(NVS)`.
4. Run SD and combined stress tests.
5. Send `k`; verify dropped events and faults are zero.
6. Repeat stress ten times and compare heap/minimum/largest-block values.

Stage 1 completes when the sketch builds cleanly, all Stage 0 functions remain
working, calibration persists, and memory/event counters remain stable.
