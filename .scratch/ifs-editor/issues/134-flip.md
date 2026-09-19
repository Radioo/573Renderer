# Flip horizontally and vertically

Status: resolved

Mirroring a depth meant typing a negative scale into the inspector; Flash, where
this format's authoring comes from, has Flip Horizontal and Flip Vertical.

## Acceptance

- `Edit > Flip horizontally` and `Flip vertically` scale the chosen depth by -1
  along its own x or y axis about its anchor on the playhead's frame, as one
  undo step, for owned and baked depths. Flipping twice restores the placement
  exactly. Tested in `document_tests` and `editor_window_tests`.
