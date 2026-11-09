# Inspector rows belong to the document, not the widget

Status: resolved

Blocked by: 37.

`Window::ShowFrame` assembles the inspector's rows itself: the owned-depth
rows, the placement fields, the script fields, the camera fields, and the
keyframe rows, and it decides which of them the user may type into. That is
document logic living in a Qt widget, which the spec says should not happen:
"Document and panel logic stays out of the Qt widgets so it can be tested
without a window".

Nothing tests it, and it has already been wrong once. `fields =
PlacementFields(...)` overwrote the "Owned by the project" row the editor docs
say is shown, and the row was missing for as long as owned depths have existed
without anything noticing.

## Acceptance

- One function over the document model returns the inspector's rows for a
  frame, given the animation, the selected depth, the frame, the authored depth
  when the project owns one, and the selected keyframe.
- Each row carries whether it can be edited and what an edit to it means, so
  the widget neither derives editability from the row's name nor decides which
  setter an edit reaches.
- `Window::ShowFrame` becomes the call plus filling the table, and
  `Window::ApplyFieldEdit` dispatches on what the row says rather than on its
  name.
- Tested under `ci`, including that an owned depth keeps its own rows alongside
  the placement fields, that only the keyframe value is editable on an owned
  depth, and that a depth holding nothing on a frame says so.
