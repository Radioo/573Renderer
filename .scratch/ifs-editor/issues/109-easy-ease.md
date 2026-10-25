# Easy ease, and keeping the keyframe selection

Status: resolved

Easing a stretch took a trip through the bezier dialog for each keyframe, and
any edit of an owned depth dropped the keyframe selection, so the selection had
to be made again after every ease from the menu.

## Acceptance

- F9, Shift+F9 and Ctrl+Shift+F9, and the lane menu's `Easy ease` submenu, ease
  both sides, the way in or the way out of the selected keyframes, turning the
  stretches into beziers that keep the side not eased, as one undo step. They
  are refused for a property that only holds and when no selected keyframe has
  a neighbour on that side. Tested in `document_tests` and
  `editor_window_tests`, and seen to fail without either slow control point,
  the straight side, either side check, the neighbour rule, the stepped check,
  the F9 shortcut or the submenu's `In`.
- An edit of an owned depth keeps the keyframe selection. The window test saw
  it dropped after an ease from the menu and after F9 before the fix, and fails
  without the restore.
