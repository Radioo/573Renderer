# Retiming keyframes in the graph

Status: resolved

The graph panel could only change a keyframe's value; moving it in time meant
going back to the timeline.

## Acceptance

- Dragging a keyframe sideways in the graph moves it to another frame, held
  between its neighbours and inside the owned frames, and Shift keeps a drag to
  the way it moved most, as one undo step that leaves the keyframe focused on
  its new frame. A move onto another keyframe is refused. Tested in
  `editor_widget_tests` and `editor_window_tests`, and seen to fail without
  either neighbour bound, the live frame, either Shift rule, the frame check,
  the bound, the refocus, the retime or the value's new frame.
