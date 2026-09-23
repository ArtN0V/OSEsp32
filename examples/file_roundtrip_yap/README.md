# File round-trip YAP

This API 1.1 example exercises `app:/`, `data:/`, system text input, Open/Save,
transaction commit/cancel, queued buttons and an initial Open-with document.
Build all examples from the project root:

```text
python tools/build_yap_examples.py
```

Copy `build/file_roundtrip.yap` to `/OSEsp32/Apps`. The detailed physical test
sequence is in `docs/STAGE_4.md`. Its `txt`/`bmp` associations intentionally
conflict with Document info and the built-in image viewer.
