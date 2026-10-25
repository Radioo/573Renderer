# Sequencing depths

Status: resolved

Laying several spans end to end, a run of images or shots, meant dragging each
one into place by hand and counting frames.

## Acceptance

- `Sequence N depths one after another` in the timeline menu, with several
  depths chosen, puts their spans under the cursor end to end in depth order as
  one undo step, moving project-owned records with them. It is refused, leaving
  the clip alone, for one depth, a depth showing nothing there, or a move out of
  the clip or into another span. Tested in `document_tests` and
  `editor_window_tests`, and seen to fail without the depth order, the
  two-depth rule, the running end, the next-frame start, committing the copy,
  the record move, the ownership lookup, the save or the menu entry.
