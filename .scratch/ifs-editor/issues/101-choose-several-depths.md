# Choosing and moving several depths

Status: resolved

Blocked by: 98.

Only one depth could be chosen at a time, so lining up a group meant moving
each depth by the same amount by hand.

## Acceptance

- Ctrl and Shift clicks on depth numbers choose several depths, and clicking a
  number never moves the playhead. Tested in `editor_widget_tests`, and seen to
  fail when Ctrl replaces the choice or the click seeks.
- Every chosen depth is outlined and a group move does not snap to its own
  members. Tested in `editor_widget_tests`, and seen to fail without the
  outlines or the filter.
- A group moves as one undo step, owned depths included, and picking a member
  keeps the group. Tested in `editor_window_tests`, and seen to fail without the
  group move, the kept pick or storing the owned depth.
