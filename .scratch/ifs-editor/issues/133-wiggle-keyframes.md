# Wiggle keyframes

Status: resolved

Jitter (a camera shake, a nervous object) meant typing a run of keyframes with
made-up offsets; After Effects has the Wiggler keyframe assistant.

## Acceptance

- `Wiggle N keyframe(s)...` in the keyframe lane menu asks for a spacing and a
  magnitude and puts a keyframe every that many frames between each property's
  first and last selected keyframe, holding the old curve's value plus a
  random offset within the magnitude for each value, as one undo step. The
  ends stay; selected keyframes between are replaced. Tested in
  `document_tests`, including that one seed always gives the same wiggle.
- Missing, unselected-between and unknown-property keyframes, a spacing of 0,
  a magnitude of 0 or less, a gap with no room and a selection with nothing to
  wiggle are refused, changing nothing.
- The new keyframes are selected with the ends, and cancelling either question
  changes nothing. Tested in `editor_window_tests`.
