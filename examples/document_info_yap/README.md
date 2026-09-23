# Document info YAP

This fullscreen API 1.1 example shows the size of a user-selected TXT/BMP and
demonstrates that a fullscreen application supplies its own Exit button. It has
read permission only and cannot create or replace a document.

Build with `python tools/build_yap_examples.py`, copy
`build/document_info.yap` to `/OSEsp32/Apps`, then use Files → a TXT/BMP → Open
with. Holding the top-left corner remains emergency recovery, not normal exit.
