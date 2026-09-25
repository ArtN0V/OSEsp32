# Paint YAP

API 1.5 Paint storage/memory reference for the no-PSRAM CYD target. It exercises the
system-owned indexed 4-bit Canvas, coordinate touch events, bounded line/brush
drawing, pencil/eraser, five thicknesses, eight palette choices, clear and
dirty-exit confirmation.

Open accepts uncompressed 16/24/32-bit BMP through the system document picker,
proportionally reduces oversized input and quantizes it to the fixed palette.
Save emits a transactional 24-bit
BMP; the Lua app commits the handle only after native scanline export succeeds.
It declares document open/create/replace capabilities and the `bmp`
association, but receives neither raw SD paths nor pixel memory.

```text
python tools/yap.py build examples/paint_yap -o build/paint.yap
```

Copy `build/paint.yap` to `/OSEsp32/Apps`. The real-card round-trip, removal and
endurance checks are listed in `docs/STAGE_5.md`.
