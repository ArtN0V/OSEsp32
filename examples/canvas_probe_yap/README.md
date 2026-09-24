# Canvas Probe YAP

Temporary API 1.3 hardware experiment for Stage 5. It compares one system-owned
320x204 buffer in RGB565, indexed 8-bit and indexed 4-bit formats. Each probe
reports the actual buffer size, active free heap, largest block, minimum free
heap, allocation/fill time and average/maximum interval for 30 dirty-rectangle
frames.

The application must remain `exclusive`: the probe deliberately measures the
largest foreground memory budget after the desktop and wallpaper cache have
been released. It requests no file capability and exposes no native pointer to
Lua.

Build with:

```text
python tools/yap.py build examples/canvas_probe_yap -o build/canvas_probe.yap
```

Run RGB, I8 and I4, press FREE after each result, and record both active and
released free heap/largest block. Then exit and relaunch ten times. A successful
host build is not a substitute for these board measurements.
