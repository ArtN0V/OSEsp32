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
   - Give English and Russian the same four-row structure, control-key colors
     and key proportions. Russian includes `ё`, `ъ`, Space, Enter, arrows,
     hide, mode and OK keys.

7. **Settings and localization**
   - Replace the flat group of setting buttons with a Windows-style category
     list and separate Display, Language and Touch pages.
   - Keep English as the first-boot default and persist English/Russian choice
     in NVS; restart after a change so the complete LVGL theme is rebuilt with
     the correct font.
   - Embed only ASCII and Cyrillic glyph ranges at 12 and 14 pixels, and use a
     compact Russian keyboard map in Russian mode. Enable LVGL's compressed
     font decoder because the generated glyph bitmaps use that format.
   - Offer six persistent desktop gradients on a separate Display page; keep
     them behind wallpaper rather than deleting wallpaper when color changes.

8. **Notes**
   - Replace the Text Input demo with a scrollable three-column gallery of
     rounded preview cards; the first card always creates a note.
   - Store bounded title/body text as `.note` documents under
     `/OSEsp32/Notes` and replace saved files transactionally.
   - Provide a fullscreen editor with a bold title, word-wrapped scrollable
     body, touch cursor placement, localized keyboard, explicit save and a
     three-way unsaved-change dialog.

9. **Date, time and screen saver**
   - Make the main Settings list vertically scrollable and add Date & time and
     Screen saver categories.
   - Keep manual local time plus UTC offset behind a `DateTimeService`; reserve
     `setUtc()` as the later NTP entry point without enabling Wi-Fi in Stage 3.
   - Trigger the saver from LVGL inactivity and provide persistent arrow-based
     selection between clock, picture-only and Windows-style starfield modes.
     Draw the animated stars in one custom layer with fixed state rather than
     allocating a widget per star.
   - Destroy the saver tree and exact image cache entry on wake. Suppress it
     while any fullscreen application is active.

## Acceptance checks on the board

1. Open Notes in English and Russian and confirm every keyboard row, including
   Space/Enter, `ё`/`ъ`, mode, arrows, hide and OK, is visible and uses the
   same control-key colors and proportions.
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
9. With wallpaper cleared, select every desktop color and restart on one of
   them. Confirm the selection persists. Set wallpaper, change color, then
   clear wallpaper and confirm the newly selected color appears.
10. With an SD card inserted, create enough notes to scroll the gallery. Check
    title ellipsis, preview clipping, word wrapping, tap-to-place cursor,
    keyboard hide/show, explicit save and all three dirty-exit choices. Reboot
    and reopen the notes. Remove the card while editing and confirm unsaved
    text is not silently discarded.
11. Set date/time and a negative UTC offset, scroll while adjusting every
    field, and reboot. Enable the screen saver for each timeout and use both
    arrows to test clock, picture-only and animated starfield modes. Wake each
    by touch and confirm none appears over the fullscreen note editor. Remove
    the SD card and confirm picture mode fails safely to black.

## Exit criterion

All eleven checks pass without a crash, stuck SD mount, clipped keyboard row,
touch inversion or persistent heap loss. The `.yap` application runtime remains
the explicit start of Stage 4.
