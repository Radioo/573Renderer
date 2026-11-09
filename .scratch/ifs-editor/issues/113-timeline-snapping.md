# Snapping bars on the timeline

Status: resolved

Lining a span up with another span, the playhead or a label meant dragging by
eye, frame by frame, and pressing a bar moved the playhead to where it was
grabbed.

## Acceptance

- Holding Shift while moving or trimming a bar snaps its nearest end to where
  other spans start or end, the playhead, the labels, or the clip's ends, within
  8 pixels, and the drag outline shows it before the release. Pressing a bar no
  longer seeks unless it is released without a drag. Tested in
  `document_tests` and `editor_widget_tests`, and seen to fail without skipping
  the dragged span, the sorting, the reach bound, either tie rule, the end
  edge, each mark, either trim rule, the reach, the Shift check in the drag, the
  deferred seek or choosing the depth on the press.
