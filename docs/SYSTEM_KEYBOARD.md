# System keyboard architecture

## Why the previous approach was rejected

The previous Notes implementation created and controlled an `lv_keyboard`
directly. Its focus event was confirmed because the toolbar's hide-keyboard
button changed state, and source inspection confirmed non-zero geometry and
valid English and Russian maps. Explicit opacity, foreground ordering and
moving the keyboard to the screen root did not make the matrix visible on the
target board.

This means another Notes-specific visibility patch would preserve the wrong
ownership model even if it happened to work. The replacement must be an
isolated system component with its own physical test screen.

## Boundary and ownership

`SystemKeyboard` is implemented under `src/ui/`. It is a system overlay, not a
Notes widget and not part of a third-party application.

- Exactly one `SystemKeyboard` instance exists.
- During the incremental Stage 3.1 refactor it may be constructed by
  `DesktopShell`; its public API must allow ownership to move later to
  `OSEsp32App` without changing applications.
- Only `SystemKeyboard` creates, hides or deletes its LVGL root and key matrix.
- Applications never retain the keyboard LVGL pointer.
- The keyboard lives on a dedicated system overlay layer above the active app.
- Calibration, diagnostics, exclusive-app teardown and shell shutdown call
  `shutdown()` and leave no keyboard timers or LVGL objects alive.

## Rendering decision

Version 1 uses a plain `lv_buttonmatrix`, not `lv_keyboard`. This removes the
widget's global mutable layout table, implicit textarea focus management and
hidden default event behavior. `SystemKeyboard` dispatches keys explicitly.

The matrix has fixed, instance-owned maps and control arrays for:

- English lower and upper case;
- Russian lower and upper case;
- shared numbers/symbols;
- shared Space, Enter, Backspace, cursor arrows, hide and Done controls.

English and Russian use the same four-row geometry and one style definition.
The first implementation has no slide animation; deterministic visibility and
touch behavior are more important than animation.

The wide space key displays the active keyboard language. A short tap inserts
one space. Holding it for at least 300 ms and moving horizontally by at least
32 pixels switches English/Russian without changing the interface language.
Both directions toggle with the current two-language set; the signed direction
is retained for a future ordered language list. The gesture disables repeat
and suppresses the release-space event, so switching never inserts a stray
space.

## Client contract

The keyboard does not require an application to expose an LVGL object. It uses
an adapter interface conceptually equivalent to:

```cpp
class TextInputClient {
 public:
  virtual void insertUtf8(const char* text) = 0;
  virtual void backspace() = 0;
  virtual void moveCursor(int8_t direction) = 0;
  virtual void enter() = 0;
  virtual void done() = 0;
  virtual void keyboardVisibilityChanged(bool visible,
                                         uint16_t coveredHeight) = 0;
};
```

Stage 3.1 supplies an `LvglTextareaInputClient` adapter. The isolated Keyboard
Test currently owns the first adapter. After its physical acceptance, Notes
will own another adapter and may switch its target between title and body;
`SystemKeyboard` never owns either textarea. A future YAP adapter will translate
the same operations into runtime events without exposing LVGL pointers or
native memory.

`show(client, options)` is idempotent and replaces the previous session only
after notifying it. `hide(reason)` detaches the client before changing LVGL
objects. `shutdown()` is safe to call repeatedly.

## State machine

```text
Detached --begin--> Hidden
Hidden --show(client)--> Visible
Visible --show(other client)--> Visible (old client detached first)
Visible --hide--> Hidden
Hidden/Visible --shutdown--> Detached
```

No callback may delete the matrix synchronously while processing a key event.
Destruction is deferred until after event dispatch or performed by the owning
update loop.

## Layout contract

- Landscape target: 320×240.
- Keyboard docks to the bottom and reports its exact covered height.
- Initial height: 112 pixels, four 26–27 pixel rows.
- The active application receives the remaining work area instead of assuming
  hard-coded textarea heights.
- System toolbar/dialog layers remain above the application but have an
  explicitly documented order relative to the keyboard.
- Rotation rebuilds the overlay after the controlled restart; there is no live
  orientation mutation.

## Diagnostics and observability

Before Notes adopts the service, the firmware exposes a minimal **System Info →
Keyboard Test** screen. It contains one textarea, Show/Hide and EN/RU controls,
a visible keyboard frame and live diagnostics showing:

- service state;
- matrix width/height and absolute coordinates;
- selected layout and key count;
- attached-client status;
- free heap and largest free block before/after show/hide.

If matrix creation or layout validation fails, report `InternalError` and show
a system dialog instead of presenting a hide button for an invisible keyboard.

## Implementation sequence

1. **Implemented and initially verified on hardware:** build `SystemKeyboard`
   with a custom `lv_buttonmatrix` and add the isolated Keyboard Test.
2. **Implemented and initially verified on hardware:** add Russian, case
   switching and symbols with one geometry/style definition.
3. **Implemented and initially verified on hardware:** add the LVGL textarea
   adapter and explicit text, cursor, Enter, Backspace, Hide and Done dispatch.
4. **Implemented; integration retest required:** Notes requests the service
   through its own adapter. All `noteKeyboard_`, `lv_keyboard` and duplicate
   layout code has been removed from the shell.
5. Test repeated show/hide, field switching, window close and SD removal while
   recording heap and largest-block baselines.
6. Move construction to the graphical composition root when `DesktopShell` is
   split; keep the application-facing contract unchanged.

## Physical acceptance gate

Stage 3.1 cannot close until all of these pass in both languages:

1. Keyboard Test shows all four rows immediately.
2. Every key produces the expected UTF-8 text; Space and Enter are visible.
3. Hide/show works 100 times without a missing matrix or declining heap trend.
4. Switching title/body moves the cursor and preserves text.
5. Closing Notes while visible and hidden leaves no touch-blocking overlay.
6. Rotation 0/180 and touch calibration remain correct.
7. Fullscreen/exclusive transitions can force `shutdown()` and reclaim the
   entire keyboard object tree.
