# Alt-dragging to stretch keyframes

Status: resolved

Keyframes could be time-stretched only through a dialog asking for a
percentage, from the earliest selected keyframe; After Effects stretches a
selection by Alt-dragging either end of it, watching the result.

## Acceptance

- Alt-dragging the first or last selected keyframe stretches the selection
  with the other end fixed, previewed while dragging, as one undo step on
  release. The dragged keyframe lands exactly under the pointer, so the
  stretch is a whole-number ratio around an anchor rather than a percentage;
  the dialog passes its percentage as `percent / 100` around the earliest
  selected keyframe. Tested in `document_tests` (a later anchor pulling
  earlier keyframes in, rounding before the anchor).
- An Alt-drag of a middle keyframe or of a lone selected one moves the
  selection as a plain drag does. Tested in `editor_widget_tests`, with the
  preview read from the drawn timeline, and the edit in
  `editor_window_tests`.
