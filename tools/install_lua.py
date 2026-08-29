#!/usr/bin/env python3
"""Install the pinned official Lua sources required by OSEsp32."""

from __future__ import annotations

import hashlib
import io
import tarfile
import urllib.request
from pathlib import Path

VERSION = "5.4.9"
URL = f"https://www.lua.org/ftp/lua-{VERSION}.tar.gz"
SHA256 = "2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6"
ROOT = Path(__file__).resolve().parents[1]
DESTINATION = ROOT / "src" / "vendor" / "lua549"

CORE = {
    "lapi.c", "lcode.c", "lctype.c", "ldebug.c", "ldo.c", "ldump.c",
    "lfunc.c", "lgc.c", "llex.c", "lmem.c", "lobject.c", "lopcodes.c",
    "lparser.c", "lstate.c", "lstring.c", "ltable.c", "ltm.c",
    "lundump.c", "lvm.c", "lzio.c",
}
SAFE_LIBRARIES = {
    "lauxlib.c", "lbaselib.c", "lcorolib.c", "lmathlib.c", "lstrlib.c",
    "ltablib.c", "lutf8lib.c",
}


def main() -> None:
    print(f"downloading official Lua {VERSION}...")
    with urllib.request.urlopen(URL, timeout=30) as response:
        archive = response.read()
    digest = hashlib.sha256(archive).hexdigest()
    if digest != SHA256:
        raise SystemExit(f"SHA-256 mismatch: expected {SHA256}, received {digest}")
    DESTINATION.mkdir(parents=True, exist_ok=True)
    wanted_sources = CORE | SAFE_LIBRARIES
    installed = []
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:gz") as package:
        prefix = f"lua-{VERSION}/src/"
        for member in package.getmembers():
            if not member.isfile() or not member.name.startswith(prefix):
                continue
            filename = member.name[len(prefix):]
            if "/" in filename:
                continue
            if not (filename.endswith(".h") or filename in wanted_sources):
                continue
            source = package.extractfile(member)
            if source is None:
                raise SystemExit(f"could not extract {member.name}")
            (DESTINATION / filename).write_bytes(source.read())
            installed.append(filename)
    # Lua's numeric representation is part of its ABI, so the official
    # configuration asks embedded ports to change this setting in luaconf.h
    # instead of passing a compiler flag. Keep every C/C++ unit on the same
    # compact 32-bit integer/float configuration for ESP32.
    config_path = DESTINATION / "luaconf.h"
    config = config_path.read_text(encoding="utf-8")
    expected = "#define LUA_32BITS\t0"
    if expected not in config:
        raise SystemExit("unexpected Lua 5.4.9 luaconf.h")
    config_path.write_text(
        config.replace(expected, "#define LUA_32BITS\t1", 1),
        encoding="utf-8",
    )
    marker = DESTINATION / "INSTALLED.txt"
    marker.write_text(
        f"Lua {VERSION}\n{URL}\nSHA-256 {SHA256}\n"
        "Only the core and OSEsp32 safe-library source set is installed.\n",
        encoding="utf-8",
    )
    print(f"installed {len(installed)} source/header files in {DESTINATION}")


if __name__ == "__main__":
    main()
