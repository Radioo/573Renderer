# Clipboard keys for depths, and cut

Status: resolved

Ctrl+C and Ctrl+V only handled keyframes; copying a depth took the timeline
menu, and nothing could be cut.

## Acceptance

- Ctrl+C, Ctrl+X and Ctrl+V act on the selected keyframes, or on the chosen
  depth when no keyframe is selected; a cut deletes only after the copy
  worked, and a paste pastes whatever was copied last. Tested in
  `editor_window_tests`, and seen to fail without either record of what was
  copied, either cut's delete, the depth copy, the paste choice, the cut
  shortcut or the keyframe copy.
