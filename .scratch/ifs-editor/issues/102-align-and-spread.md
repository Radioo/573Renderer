# Aligning and spreading chosen depths

Status: resolved

Blocked by: 101.

Several depths could be moved together, but lining them up with each other
still meant dragging each one into place.

## Acceptance

- Offsets align outlines to an edge or middle of the selection, and spread
  centres evenly between the outermost ones. Tested under `ci`, and seen to fail
  with the wrong target or step.
- The Edit menu aligns and spreads the chosen depths as one undo step, each by
  its own offset, and refuses too few. Tested in `editor_window_tests`, and seen
  to fail with one offset for every depth or without the minimum.
