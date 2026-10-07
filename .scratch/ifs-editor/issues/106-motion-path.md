# Motion path on the stage

Status: resolved

Nothing on the stage showed where a depth goes over time, so judging a move
meant stepping frame by frame.

## Acceptance

- The selected depth's motion path is drawn over the stage for the span under
  the playhead, with a dot per frame and a box on each owned `Translation`
  keyframe, switchable from `View > Motion path` (remembered, on by default),
  and absent for a hidden depth or a 3D span. Tested in `document_tests`,
  `editor_widget_tests` and the live `editor_window_tests`, and seen to fail
  without the pixel scale, the 3D check, the `Translation` match, the keyed
  check, the span start, the drawing, the line, the refresh, the toggle, the
  default, the hidden check or the edited clip.
