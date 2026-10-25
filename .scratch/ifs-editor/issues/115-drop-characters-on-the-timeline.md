# Dropping characters on the timeline

Status: resolved

A character from the library could only be dropped on the stage, which always
placed it from the playhead; starting one at a chosen frame took the menu and
a dialog.

## Acceptance

- Dragging a character from the library onto the timeline starts a depth
  showing it from the frame it lands on to the clip's end, on the row's own
  depth when that depth is free for the stretch and on a new depth otherwise,
  as one undo step. Drops on the gutter, on an empty timeline, of another
  format or of a payload that is not a character are not taken. Tested in
  `editor_widget_tests` and `editor_window_tests`, and seen to fail without the
  frame, the gutter and empty rules, the number check, the lane rule, the
  free-row check, the fallback or the connection.
