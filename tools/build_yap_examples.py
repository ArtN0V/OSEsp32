#!/usr/bin/env python3
"""Build the runnable and failure-path YAP examples in one command."""

from pathlib import Path

import yap_pack


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "examples" / "hello_yap" / "manifest.json"
EXAMPLES = {
    "hello.yap": ROOT / "examples" / "hello_yap" / "main.lua",
    "test_infinite_loop.yap": ROOT / "examples" / "yap_runtime_tests" / "infinite_loop.lua",
    "test_out_of_memory.yap": ROOT / "examples" / "yap_runtime_tests" / "out_of_memory.lua",
    "test_syntax_error.yap": ROOT / "examples" / "yap_runtime_tests" / "syntax_error.lua",
    "test_missing_entry.yap": ROOT / "examples" / "yap_runtime_tests" / "missing_entry.lua",
}


def main() -> None:
    output = ROOT / "build"
    for filename, source in EXAMPLES.items():
        yap_pack.pack(MANIFEST, source, output / filename)
    for mode in ("windowed", "fullscreen", "exclusive"):
        yap_pack.pack(ROOT / "examples/lifecycle_yap" / f"{mode}.json",
                      ROOT / "examples/lifecycle_yap/main.lua",
                      output / f"lifecycle_{mode}.yap")
    for example in ('file_roundtrip_yap','document_info_yap','calculator_yap',
                    'canvas_probe_yap','paint_yap'):
        yap_pack.pack(ROOT/'examples'/example/'manifest.json',ROOT/'examples'/example/'main.lua',
                      output/f'{example.removesuffix("_yap")}.yap')
    print(f"built {len(EXAMPLES) + 8} packages in {output}")


if __name__ == "__main__":
    main()
