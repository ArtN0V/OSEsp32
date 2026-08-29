# Hello YAP

Build the deterministic package from the repository root:

```text
python3 tools/yap_pack.py pack examples/hello_yap/manifest.json \
  examples/hello_yap/main.lua -o build/hello.yap
```

Copy `hello.yap` to `/OSEsp32/Apps` on the SD card and open it in Files. The
current Stage 4 slice validates and displays the package metadata but does not
execute Lua yet. `osesp32.ui.label` is the intended first runtime API.
