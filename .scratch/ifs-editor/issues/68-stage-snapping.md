# Snapping while moving on stage

Status: resolved

Blocked by: 67.

Objects could only be lined up by eye or by typing numbers.

## Acceptance

- A move snaps an object's edges and centre to the stage's and to other
  objects' within a reach, the nearest line winning. Tested under `ci`.
- The viewport snaps drags, draws the guide while dragging, and moves freely
  with Alt. A widget test covers all three and was seen to fail with snapping
  switched off.
- The View menu turns snapping off and remembers it. Covered by a window test.
