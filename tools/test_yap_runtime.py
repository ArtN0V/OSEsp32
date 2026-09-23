#!/usr/bin/env python3
"""Compile the real runtime and vendored Lua against minimal host I/O stubs."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    with tempfile.TemporaryDirectory(prefix="osesp32-runtime-") as temp:
        output = Path(temp)
        objects = []
        flags = ["-g", "-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        for source in sorted((ROOT / "src/vendor/lua549").glob("*.c")):
            obj = output / (source.stem + ".o")
            subprocess.run(["cc", *flags, "-c", str(source), "-o", str(obj)], check=True)
            objects.append(str(obj))
        executable = output / "runtime-test"
        subprocess.run(["c++", "-std=c++11", *flags,
                        "-I", str(ROOT / "tests/runtime_host"),
                        str(ROOT / "src/runtime/YapRuntimeService.cpp"),
                        str(ROOT / "tests/runtime_host/runtime_test.cpp"),
                        *objects, "-lm", "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True, timeout=20)

if __name__ == "__main__":
    main()
