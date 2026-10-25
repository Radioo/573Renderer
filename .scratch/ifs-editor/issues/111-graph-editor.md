# Graph editor

Status: resolved

The timeline showed where keyframes were but not what their values did between
them, and changing a value meant typing it into the inspector.

## Acceptance

- A Graph panel beside the timeline draws the focused property of the selected
  owned depth over its frames, one line per value with a box per keyframe and
  the playhead, and nothing for a stepped property or a depth the project does
  not own. Dragging a box sets that one value as one undo step, and clicking
  seeks. Tested in `document_tests` (`GraphedTrack`, `SetKeyValuesAt`),
  `editor_widget_tests` and `editor_window_tests`, and seen to fail without the
  stepped rule, the axis direction, the grab reach, the dragged component, the
  change check, the focus, the rounding, the boxes, the line, the live drag,
  the empty graph, the value edit, the signal connections or the dock.
