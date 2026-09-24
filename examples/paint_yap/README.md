# Paint YAP

First API 1.4 Paint slice for the no-PSRAM CYD target. It exercises the
system-owned indexed 4-bit Canvas, coordinate touch events, bounded line/brush
drawing, pencil/eraser, five thicknesses, eight palette choices, clear and
dirty-exit confirmation.

BMP Open/Save is deliberately the next slice: this package currently declares
no storage capability and does not claim that a drawing can be saved.

```text
python tools/yap.py build examples/paint_yap -o build/paint.yap
```
