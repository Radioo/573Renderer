# Splitting a span at the playhead

Status: resolved

Cutting a span in two, to move or trim one part on its own, was not possible
without rebuilding the second part by hand.

## Acceptance

- `Edit > Split depth at the playhead` (Ctrl+Shift+D) and the timeline menu
  split the selected depth's span into two on the same depth as one undo step,
  and every frame draws what it drew before. It is refused on a span's first
  frame, for a character that is not an image or a shape (a new instance would
  start again), and for a project-owned depth. Tested in `document_tests` and
  `editor_window_tests`, and seen to fail without the first-frame check, either
  still-character check, following swaps, relabelling the closing remove, the
  trim order, a truly unused scratch depth, the ownership check, the shortcut
  or the menu entry.
