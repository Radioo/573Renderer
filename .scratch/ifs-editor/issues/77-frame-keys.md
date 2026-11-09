# Stepping frames from the keyboard

Status: resolved

Blocked by: 39.

The playhead could only be moved with the mouse.

## Acceptance

- Page Up and Page Down step a frame, Home and End go to the ends of the clip,
  and J and K go to the previous and next change on the selected depth,
  without leaving the clip. Tested under `ci` (`timeline_tests`,
  `editor_window_tests`), and the window test seen to fail with K stepping
  backwards.
