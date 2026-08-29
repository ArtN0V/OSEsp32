# YAP runtime failure tests

Run `python3 tools/build_yap_examples.py` from the repository root and copy the
four `test_*.yap` files from `build/` to `/OSEsp32/Apps` on the SD card.

Expected result after pressing **RUN**:

| Package | Expected status |
|---|---|
| `test_infinite_loop.yap` | `limit_exceeded` within about 250 ms |
| `test_out_of_memory.yap` | `out_of_memory` |
| `test_syntax_error.yap` | `compile_error` |
| `test_missing_entry.yap` | `entry_missing` |

Every result screen must report `after close: 0 B`. Run each package repeatedly
and verify that the before/after heap values stabilize instead of continually
falling.
