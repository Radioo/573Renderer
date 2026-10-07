# Dragging labels on the ruler

Status: resolved

Moving a label meant right-clicking the frame it should go to and choosing it
from the menu.

## Acceptance

- A label pressed on the ruler within 4 pixels of its line can be dragged to
  another frame, drawn live, and letting go moves it as one undo step. A click
  on it still seeks, a drag back to its own frame changes nothing, and presses
  elsewhere keep their meaning. Tested in `editor_widget_tests` and
  `editor_window_tests` (through the saved IFS), and seen to fail without the
  ruler rule, the tight reach, the drag threshold, the same-frame guard, the
  seek on a click, the live drawing, the connection or the frame.
