# Time-reversing keyframes

Status: resolved

Playing part of an owned depth's animation backwards meant retyping every
keyframe at its mirrored frame and working out the eases by hand.

## Acceptance

- `Time-reverse N keyframe(s)` in the keyframe lane menu mirrors the selected
  keyframes of each property within their own first and last frames, moving
  each ease with the stretch it describes and reflecting beziers, as one undo
  step that keeps the selection and focus on the moved keyframes. It is refused
  when an unselected keyframe lies between the selected ones or a selected one
  does not exist. Tested in `document_tests` (every frame mirrored within one
  integer step) and `editor_window_tests`, and seen to fail without the bezier
  reflection, the between check, the ease hand-over, the last keyframe's ease,
  the sort, the two-key rule, the existence check, the returned order, the menu
  entry, the new selection or the focus.
