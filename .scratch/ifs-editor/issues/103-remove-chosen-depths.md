# Removing the chosen depths

Status: resolved

Blocked by: 101.

Only one depth could be removed at a time, Delete did nothing without a
keyframe selection, and removing a project-owned depth left the project a record
for a depth that was no longer placed.

## Acceptance

- The chosen depths are removed at the frame as one undo step, from the timeline
  menu or with Delete when no keyframe is selected, and the removal is refused
  when the project owns any of them. Tested in `editor_window_tests`, and seen
  to fail when only the first depth is removed, without the ownership check, or
  with Delete left to keyframes.
