# Scaling and turning objects in the viewport

Status: resolved

Blocked by: 51.

The viewport could select and move an object, but scale and rotation still had
to be typed into the inspector.

## Acceptance

- The document scales an object along its own axes and turns it about its
  anchor without moving the anchor, for baked placements (keeping short forms
  while they fit) and owned depths (keying Scale and Rotate skew). Tested under
  `ci`.
- The geometry for a dragged corner and a dragged turn handle, and the preview
  outline, live in the document and are tested under `ci`.
- The selected object shows corner handles, a turn handle and its anchor;
  dragging them scales or turns it as one undo step.
- The timeline shades the selected depth.
