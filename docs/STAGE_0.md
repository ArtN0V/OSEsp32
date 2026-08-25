# Roadmap Stage 0 — hardware qualification

## Goal

Produce a measured hardware profile and demonstrate that the display, touch,
SD card and on-board peripherals work reliably together. Stage 1 must not
begin until failures are either fixed or explicitly documented.

## Preparation

1. Use a stable 5 V USB supply and a known data-capable cable.
2. Insert a backed-up FAT32 microSD card, ideally 4–32 GB for the first test.
3. Upload the diagnostic sketch.
4. Open Serial Monitor at 115200 baud and save the complete output.
5. Keep the stylus available; resistive touch requires physical pressure.

## Test 0.1 — boot and hardware identity

Run command `m`.

Record:

- chip model and revision;
- core count and CPU frequency;
- flash capacity and clock;
- PSRAM total;
- free heap, minimum heap and largest allocation block;
- compiled sketch size and available application space;
- reset reason.

Acceptance:

- chip identifies as a classic dual-core ESP32;
- selected flash size equals the reported physical flash size;
- boot is repeatable for ten resets;
- no brownout or watchdog reset occurs while idle.

## Test 0.2 — TFT and orientation

Run command `d` or press **DISPLAY**.

Observe solid red, green, blue and white frames, followed by a grid with a
complete outer border and two diagonals.

Acceptance:

- colors are correct and not swapped;
- landscape text is upright;
- the entire 320x240 area is visible;
- no persistent flicker, noise, clipped edge or offset is present.

If red and blue are swapped, change `rgb_order` in `DisplayDriver.cpp`. Do not
raise the 27 MHz TFT clock until the full stress test passes.

## Test 0.3 — backlight

Run command `b`.

Acceptance:

- brightness visibly changes through approximately 25%, 60% and 100%;
- the direction is correct;
- there is no reset, high-pitched instability or complete inversion.

If brightness direction is reversed, change `light_.config().invert` rather
than adding inversions throughout the application.

## Test 0.4 — raw touch and calibration

Run command `t`. Press the four marked corners, center, and several points near
each edge. The screen and Serial Monitor show raw X/Y, pressure and mapped X/Y.
Send `q` to leave the test.

Acceptance:

- touch is detected everywhere on the active area;
- screen coordinates move in the same direction as the stylus;
- corner positions map close to their expected screen corners;
- idle operation produces no phantom touches;
- pressure is comfortably above 300 during a normal stylus press.

Record raw values with a normal press:

| Point | Raw X | Raw Y | Pressure | Screen X/Y |
|---|---:|---:|---:|---|
| top-left |  |  |  |  |
| top-right |  |  |  |  |
| bottom-left |  |  |  |  |
| bottom-right |  |  |  |  |
| center |  |  |  |  |

Use conservative limits slightly inside the absolute extremes. A later
calibration wizard will store per-device values in NVS.

## Test 0.5 — SD card

Run command `s`.

The firmware mounts the card at 10 MHz, writes known content to a temporary
file, reads it back, compares it byte-for-byte and removes the file.

Acceptance:

- mount, write, read and content checks all report `PASS`;
- card type and capacity are plausible;
- `/yellowos_stage0.tmp` is absent after completion;
- repeating the test ten times produces no failure.

On failure, try another FAT32 card before changing code. Then verify CS GPIO5
and the GPIO18/19/23 SPI mapping. Only after stable operation should higher SD
clock rates be tested.

## Test 0.6 — on-board I/O

Run command `o`.

Acceptance:

- LED visibly shows red, green and blue in that order;
- speaker emits two different tones;
- covering and uncovering the light sensor changes its raw ADC value clearly.

Repeat once with the light sensor covered and record both values:

| Measurement | ADC raw |
|---|---:|
| uncovered |  |
| covered |  |

## Test 0.7 — combined bus and memory stress

First run `s` successfully, then run `x`. Move the stylus over the screen while
the progress animation runs.

The test performs 600 display updates, continuous touch polling and 20 SD
appends, while recording the lowest free heap. It removes the stress file at
the end.

Acceptance:

- zero SD write errors;
- touch remains responsive;
- animation continues without corruption;
- no crash, watchdog reset or brownout;
- heap does not continuously decline on repeated runs;
- `/yellowos_stress.tmp` is removed.

Run the stress test at least ten times. Record the worst observed heap values.

## Final Stage 0 result

| Area | Result | Evidence/notes |
|---|---|---|
| Identity and memory | NOT RUN | |
| TFT | NOT RUN | |
| Backlight | NOT RUN | |
| Touch | NOT RUN | |
| SD | NOT RUN | |
| RGB/speaker/LDR | NOT RUN | |
| Combined stress | NOT RUN | |

Stage 0 is complete when every row is `PASS`, or when an intentional hardware
exception and its software workaround are recorded in `HARDWARE.md`.
