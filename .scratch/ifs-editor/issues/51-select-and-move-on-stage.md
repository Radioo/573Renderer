# Selecting and moving objects in the viewport

Status: resolved

Blocked by: 50.

A depth could only be picked in the timeline and moved by typing a
translation into the inspector.

## Acceptance

- The document works out where each depth of a clip sits on stage on a frame,
  following afp-core's own transform (matrix, translation in twentieths, held
  rotation origin, nested sprites), and finds the topmost depth under a point.
  Tested under `ci`.
- Moving a depth by a stage offset edits the live placement, carrying the
  matrix when the placement had none, or keys Translation for an owned depth.
  Tested under `ci`.
- Clicking the viewport selects the depth under the pointer and outlines it;
  dragging it moves the depth as one undo step.
- The viewport keeps the stage's aspect.
- A `local_dll` test renders a transformed placement and requires every
  changed pixel to fall inside the computed outline.
