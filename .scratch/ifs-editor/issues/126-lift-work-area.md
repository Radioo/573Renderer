# Lift the work area

Status: resolved

The work area's frames could be extracted, closing the gap, but not emptied
while keeping the timing of everything after them, After Effects' Lift Work
Area.

## Acceptance

- `Edit > Lift the work area` empties the work area's frames as one undo step.
  No frame moves; spans inside are removed, spans running in or out are
  trimmed, and a span crossing the whole work area is split around it. Every
  frame outside the work area shows what it showed. Tested in
  `document_tests`.
- Sprites and other clips that start again after the lifted frames are
  counted and reported; lifting every frame is allowed; a range outside the
  clip, or one showing nothing, is refused and changes nothing.
- The playhead and the work area stay, and the trim's refusals apply. Tested
  in `editor_window_tests`.
