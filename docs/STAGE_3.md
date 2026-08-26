# Roadmap Stage 3 — storage, files and personalization

## Goal

Make SD content useful entirely from the touch UI while retaining predictable
RAM usage on an ESP32 without PSRAM. Stage 3 also fixes the cramped keyboard
and adds persistent desktop personalization.

## Implementation order

1. **Storage ownership**
   - Give one `StorageService` ownership of the SD hardware SPI instance.
   - Mount at shell startup, probe periodically, publish mount/removal/error
     events and retry after insertion.
   - Create `/OSEsp32/Apps`, `/OSEsp32/Data` and `/OSEsp32/Wallpapers`.
   - Reject parent traversal and keep paths in fixed-size buffers.

2. **LVGL filesystem bridge**
   - Register drive `S:` with bounded read, seek and directory callbacks.
   - Keep all access cooperative in the UI loop; application code may not call
     Arduino `SD` directly when the Stage 4 runtime arrives.

3. **File manager and associations**
   - Read at most four displayed entries into RAM and paginate large folders.
   - Support directory navigation and an explicit parent action.
   - Associate BMP/JPG/JPEG with the built-in viewer; report unsupported types
     instead of attempting to execute them.

4. **Image viewer and optimized wallpaper**
   - Enable LVGL's incremental BMP and TJPGD decoders.
   - Center native-size images in the viewer and clip oversized images.
   - Convert a chosen image once into the fixed 320x204 RGB565 `OWP1`
     wallpaper format without modifying its source file.
   - Read the OWP file in 20-line strips and keep a two-strip LRU cache
     (about 25 KiB) so opening and closing windows does not repeatedly decode
     BMP/JPEG or reread the entire desktop.
   - Migrate an older path-based wallpaper automatically on first load.
   - Remove the wallpaper safely when the card is unavailable.
   - Normalize extension case for LVGL, accept baseline JPEG and recommend
     320x204 wallpapers. Progressive JPEG, scaling and PNG remain deferred
     until heap measurements prove them safe.

5. **Rotation and calibration**
   - Save orientation in NVS and restart after a setting change.
   - Rotate the ILI9341 and transform touch coordinates together.
   - Fit new touch calibration points in base orientation so one calibration
     remains valid at both 0 and 180 degrees.

6. **Keyboard layout repair**
   - Reserve 31 pixels for the text field and the remaining 123 pixels for the
     keyboard inside the window content area.
   - Reduce key padding and use the 12-pixel font so every keyboard row remains
     visible and touchable.

7. **Settings and localization**
   - Replace the flat group of setting buttons with a Windows-style category
     list and separate Display, Language and Touch pages.
   - Keep English as the first-boot default and persist English/Russian choice
     in NVS; restart after a change so the complete LVGL theme is rebuilt with
     the correct font.
   - Embed only ASCII and Cyrillic glyph ranges at 12 and 14 pixels, and use a
     compact Russian keyboard map in Russian mode.

## Acceptance checks on the board

1. Open Text Input and confirm every row, including space/Enter and mode keys,
   is fully visible and touchable.
2. Boot without SD: the desktop and Settings must work; Files shows a clear
   unavailable message.
3. Insert a FAT32 SD, wait up to three seconds, reopen Files and navigate a
   folder containing more than four entries using both page buttons.
4. Open BMP, `.jpg`, `.JPG`, `.jpeg` and `.JPEG` baseline samples. Repeat
   several times and confirm System Info does not show steadily decreasing
   minimum usable heap.
5. Set BMP and JPEG images of different sizes as wallpaper. Confirm the source
   files remain unchanged and `desktop.owp` appears under Wallpapers. Open and
   close windows repeatedly; redraws should stay responsive. Restart and
   confirm the wallpaper returns. Remove/reinsert the card and confirm there
   are no stale strips from the previous image.
6. Select Rotate to 180, allow restart, and verify all four screen corners and
   the keyboard. Rotate back to 0 and repeat.
7. Run touch calibration in the rotated orientation, restart, then verify touch
   at both orientations without recalibrating.
8. On a clean NVS boot, confirm English is selected. Switch to Русский, allow
   the restart, inspect the desktop, Settings, Files, dialogs and all rows of
   the Cyrillic keyboard, then switch back to English.

## Exit criterion

All eight checks pass without a crash, stuck SD mount, clipped keyboard row,
touch inversion or persistent heap loss. The `.yap` application runtime remains
the explicit start of Stage 4.
