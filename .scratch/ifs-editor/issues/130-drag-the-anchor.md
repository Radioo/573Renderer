# Drag the anchor on stage

Status: resolved

The anchor could be centred but not put anywhere else without typing an
origin, which moves the object; After Effects' Pan Behind moves it by
dragging, with the object staying put.

## Acceptance

- Dragging the selection's anchor cross moves the anchor to the pointer on the
  playhead's frame, over the whole span, without moving the object, as one
  undo step on release. The stage offset goes into the object's space through
  the inverse of its matrix on that frame. Tested in `document_tests` against
  every frame's outline, with refusals for a flat matrix and an offset too
  small to move the origin.
- The cross follows the pointer while dragging; a corner handle on the anchor
  still scales. Tested in `editor_widget_tests`.
- A depth the project owns is refused. Tested in `editor_window_tests`.
