# Hardware baseline

Target board: ESP32-2432S028 / 2.8-inch CYD, marked without a trailing `R`.

The known-good `CYD_Cheap_Yellow_Display/FlappyKiernan.ino` project proves that
this physical unit behaves as the common resistive-touch CYD despite its label:

| Function | Controller or GPIO | Status |
|---|---:|---|
| TFT | ILI9341, 320x240 landscape | Proven by reference project |
| TFT SCLK/MOSI/MISO | 14 / 13 / 12 | Proven |
| TFT DC/CS | 2 / 15 | Proven |
| Backlight | 21 | Proven |
| Touch | XPT2046 resistive | Proven |
| Touch CLK/MOSI/MISO | 25 / 32 / 39 | Proven |
| Touch CS/IRQ | 33 / 36 | Proven |
| SD SCLK/MISO/MOSI | 18 / 19 / 23 | Verified on the physical board |
| SD CS | 5 | Verified on the physical board |
| Speaker | 26 | Verified on the physical board |
| Light sensor | 34 ADC | Verified on the physical board |
| RGB LED R/G/B | 4 / 16 / 17, active-low | Verified on the physical board |

## SPI allocation

- SPI2/HSPI: TFT through LovyanGFX at a conservative 27 MHz write clock.
- SPI3/VSPI: SD card at an initial 10 MHz.
- Software SPI: XPT2046 touch.

This allocation lets all three peripherals operate at the same time without
reconfiguring a hardware SPI controller between unrelated GPIO sets.

## Initial touch calibration

The initial landscape mapping is inherited from the working reference:

```text
raw X: 200 .. 3700 -> screen X: 0 .. 319
raw Y: 240 .. 3800 -> screen Y: 0 .. 239
minimum pressure: 300
invert X: yes
invert Y: yes
```

The first on-device test confirmed that both raw axes run opposite to the
landscape screen axes, so `TOUCH_INVERT_X` and `TOUCH_INVERT_Y` are enabled.
The numeric limits are fallback values. Touch calibration collects values at
all four corners and the center. The diagnostic/shell five-point wizard
calculates refined limits by linear regression and stores them in the
`osesp32_touch` NVS namespace. `yellow_touch` is read only as a migration source
for early development builds.

## Expected memory profile

The common board uses an ESP32-WROOM-32 module with 4 MiB flash and no PSRAM.
OSEsp32 treats that as an expectation, not a fact. The Serial report is the
source of truth for this particular unit.
