# Hello YAP

Build the deterministic package from the repository root:

```text
python3 tools/yap_pack.py pack examples/hello_yap/manifest.json \
  examples/hello_yap/main.lua -o build/hello.yap
```

Copy `hello.yap` to `/OSEsp32/Apps` on the SD card, open it in Files and press
**RUN**. OSEsp32 starts a quota-limited Lua VM, calls `main()`, displays
`Hello from YAP!`, and destroys the VM before drawing the result window.

To build Hello and all failure-path test packages at once, run:

```text
python3 tools/build_yap_examples.py
```
