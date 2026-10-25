# Timeline with depth rows

Status: resolved

Blocked by: 20, 21.

## Acceptance

- The timeline shows one row per depth of the selected animation, with the frame range each depth is placed over, read from the animation model.
- Moving the playhead seeks the host and updates the viewport.
- Labels appear on the ruler at their frames.
- The row and range computation is Qt-free and tested under `ci`.
