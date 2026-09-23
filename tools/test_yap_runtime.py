#!/usr/bin/env python3
"""Compile the real runtime and vendored Lua against minimal host I/O stubs."""
from pathlib import Path
import subprocess
import tempfile
import struct
import zlib
import yap_pack

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
                        str(ROOT / "src/services/AppStorageService.cpp"),
                        str(ROOT / "src/services/YapPackageService.cpp"),
                        str(ROOT / "src/services/DateTimeService.cpp"),
                        str(ROOT / "tests/runtime_host/runtime_test.cpp"),
                        *objects, "-lm", "-o", str(executable)], check=True)
        fixture=output/'valid.yap'
        yap_pack.pack(ROOT/'examples/file_roundtrip_yap/manifest.json',ROOT/'examples/file_roundtrip_yap/main.lua',fixture)
        paths=[str(fixture)]
        original=fixture.read_bytes()
        for case in ('overlap','api','resource','flags','overflow','duplicate','padding'):
            data=bytearray(original)
            if case=='overlap': struct.pack_into('<I',data,56,struct.unpack_from('<I',data,36)[0])
            if case=='flags': struct.pack_into('<I',data,24,1)
            if case=='overflow': struct.pack_into('<I',data,40,0xffffffff)
            if case=='duplicate': data[72:76]=b'LUAS'
            if case in ('api','resource','padding'):
                entry=32 if case in ('api','padding') else 72
                offset,length=struct.unpack_from('<II',data,entry+4)
                if case=='api': data[offset+9]=255
                elif case=='resource': data[offset:offset+4]=b'../x'
                else: data[offset+55]=1
                struct.pack_into('<I',data,entry+12,zlib.crc32(data[offset:offset+length]))
            data[20:24]=bytes(4); struct.pack_into('<I',data,20,zlib.crc32(data))
            bad=output/f'{case}.yap'; bad.write_bytes(data); paths.append(str(bad))
        subprocess.run([str(executable),*paths], check=True, timeout=30)

if __name__ == "__main__":
    main()
