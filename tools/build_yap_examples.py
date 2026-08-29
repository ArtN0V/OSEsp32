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
    print(f"built {len(EXAMPLES)} packages in {output}")


if __name__ == "__main__":
    main()
