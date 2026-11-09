# Time-stretching keyframes

Status: resolved

Making part of an owned depth's animation faster or slower meant moving every
keyframe by hand and working out the new spacing.

## Acceptance

- `Time-stretch N keyframe(s)...` in the keyframe lane menu asks for a stretch
  factor in percent and moves every selected keyframe away from the earliest
  selected one by that share of its distance, rounded to the nearest frame, as
  one undo step. Values and eases stay with their keyframes. Tested in
  `document_tests` (sampled frames match at the stretched spacing, the anchor
  is the earliest selected frame of any property, half frames round up).
- It is refused, changing nothing, at 0%, for a missing keyframe, when a
  keyframe would leave the owned range or land on or pass another of its
  property, and when nothing moves.
- The moved keyframes stay selected and the focused one stays focused; a
  cancelled dialog changes nothing. Tested in `editor_window_tests`, and seen
  to fail without the menu entry, the cancel check, the selection or the
  focus.
