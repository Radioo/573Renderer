# Simplify keyframes

Status: resolved

Owning a depth keys every frame its properties are set on, and shipped tweens
are baked one update per frame, so an owned depth arrives with a keyframe on
every frame of its motion. Thinning that out meant deleting keyframes by hand
and checking nothing moved.

## Acceptance

- `Simplify N keyframe(s)...` in the keyframe lane menu asks for a tolerance
  and removes every selected keyframe a straight line can stand in for, keeping
  each property's first and last selected keyframe, as one undo step. Every
  frame between them samples within the tolerance of before, rounding
  included, and exactly at 0. Stretches that cannot be merged keep their ease.
  Tested in `document_tests`.
- Stepped properties are left alone; a missing or unselected-between keyframe,
  a keyframe of a property the depth does not animate, a negative tolerance
  and a selection with nothing to remove are refused, changing nothing.
- The remaining keyframes stay selected, the status bar gives the count, and
  a cancelled dialog changes nothing. Tested in `editor_window_tests`.
