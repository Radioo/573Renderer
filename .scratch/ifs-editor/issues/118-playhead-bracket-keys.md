# Moving and trimming spans to the playhead

Status: resolved

Lining a span's start or end up with the playhead meant dragging it by eye or
typing frames into the trim.

## Acceptance

- `[` and `]` move the chosen depth's span so it starts or ends on the
  playhead, and Alt+`[` and Alt+`]` trim its start or end to it, on the span
  under the playhead or the nearest one, through the same edits as the timeline
  so owned records follow. An edit that would change nothing is not made, and a
  depth with nothing in the clip is reported. Tested in `document_tests` and
  `editor_window_tests`, and seen to fail without the nearest rule, either
  distance, the depth filter, either end of the move, either trim end, the
  empty-move guard or the report.
