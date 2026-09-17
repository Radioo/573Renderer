# Nudging with the arrow keys

Status: resolved

Blocked by: 51.

Moving a depth by a single pixel took a careful mouse drag.

## Acceptance

- Arrow keys move the selected depth by one stage pixel, and by ten with
  Shift, as one undo step each; without a selection the key is not used.
  Tested in `editor_widget_tests` and `editor_window_tests`, and seen to fail
  with the move not emitted.
