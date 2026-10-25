# Fit to the stage

Status: resolved

Filling the stage with a depth meant working out a scale and a translation by
hand; After Effects has Fit to Comp and its width and height forms.

## Acceptance

- `Edit > Fit to the stage` (Ctrl+Alt+F), `Fit to the stage's width`
  (Ctrl+Alt+Shift+H) and `Fit to the stage's height` (Ctrl+Alt+Shift+G) scale
  the chosen root depth about its anchor so its box fills the animation's stage
  size, or one side of it at a kept aspect, and centre it, on the playhead's
  frame, as one undo step, for owned and baked depths alike. Tested in
  `document_tests` through the real reshape and move edits.
- A depth with no known box, a turned, skewed or flat one, and any depth
  inside a sprite are refused. Tested in `editor_window_tests`.
