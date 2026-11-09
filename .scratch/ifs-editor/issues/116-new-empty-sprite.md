# New empty sprites

Status: resolved

A sprite could only come from grouping depths that already existed or from
duplicating another sprite, so a nested animation could not be started from
nothing.

## Acceptance

- `New empty sprite...` in the library menu, with or without a character
  selected, asks for a frame count and defines an empty sprite of that length
  under the next free id as one undo step, then opens it in the clip box.
  Refused for no frames, more than 65535, or a root with no frame. Tested in
  `document_tests` and `editor_window_tests`, and seen to fail without either
  bound, the root check, the frame count, the clip box refill, the opening, the
  menu entry or its handling.
