# Calculator YAP

Reference application for OSEsp32 API 1.2. It uses only bounded system-owned
labels and buttons, receives queued events through `ui.wait()` and supplies its
own Exit button because it runs fullscreen.

Build it with:

```text
python tools/yap.py build examples/calculator_yap -o build/calculator.yap
```
