# Extract the work area

Status: resolved

The clip could be trimmed to the work area, but there was no way to take the
work area out of the clip and close the gap, After Effects' Extract Work Area.

## Acceptance

- `Edit > Extract the work area` removes the work area's frames from the clip
  as one undo step, moves the later frames up, and every remaining frame shows
  what it showed. Spans inside the cut go, spans running in or out of it are
  trimmed, and spans crossing it keep their object with the lost updates
  carried onto the frame after the cut. Tested in `document_tests`.
- Labels on the cut frames land on the frame after it; a range outside the
  clip or covering all of it is refused and changes nothing.
- The playhead keeps its content, the work area is cleared, crossing sprites
  are reported, and the trim's refusals apply. Tested in
  `editor_window_tests`.
