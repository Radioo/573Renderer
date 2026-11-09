# Toggling hold keyframes

Status: resolved

Making keyframes hold, or stop holding, took the ease submenu one keyframe
selection at a time and never said which way it would go.

## Acceptance

- Ctrl+Alt+H and `Toggle hold` in the lane menu hold every selected keyframe,
  or return them all to linear when they all hold already, as one undo step,
  refused where `SetKeysEase` would refuse. Tested in `document_tests` and
  `editor_window_tests`, and seen to fail with any-instead-of-all, the targets
  swapped, a missing track read, the shortcut or the menu entry.
