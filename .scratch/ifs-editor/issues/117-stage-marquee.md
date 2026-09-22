# Marquee selection on the stage

Status: resolved

Choosing several depths could only be done from the timeline's depth numbers;
on the stage, a drag across empty space did nothing.

## Acceptance

- Dragging across empty stage space draws a dashed marquee and chooses every
  depth whose outline it touches, by a separating axis test so a turned
  outline is judged by its body and not its bounding box. A tiny drag stays a
  click, and a drag from inside the selection still moves it. Tested in
  `document_tests`, `editor_widget_tests` and `editor_window_tests`, and seen to
  fail without the edge normals, either half of the overlap test, the box's
  vertical axis, the depth order, the start rule, the drag threshold, the band
  following the pointer, the drawing or the connection.
