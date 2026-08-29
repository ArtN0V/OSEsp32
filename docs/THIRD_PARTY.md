# Third-party notices

## Noto Sans

`src/ui/OSEsp32Font12.c` and `src/ui/OSEsp32Font14.c` contain raster glyph data
generated from **Noto Sans Regular** for the ASCII and Cyrillic Unicode ranges.
`src/ui/OSEsp32Font16Bold.c` contains the corresponding title subset generated
from **Noto Sans Bold**.

Noto Sans is Copyright 2012 Google Inc. and is distributed under the
[Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0).

The generated font data was produced with `lv_font_conv`; LVGL's Montserrat
fonts remain the fallback for symbols that are not present in the subset.

## Lua 5.4.9

`tools/install_lua.py` downloads the official Lua 5.4.9 source archive from
<https://www.lua.org/ftp/>, verifies SHA-256
`2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6` and
places a trimmed source set under `src/vendor/lua549`.

Lua is Copyright 1994–2026 Lua.org, PUC-Rio and is distributed under the
[Lua license](https://www.lua.org/license.html). OSEsp32 excludes the standard
`io`, `os`, `package` and `debug` libraries as well as native module loading.
