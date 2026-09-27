# Ungrouping a sprite

Status: resolved

Blocked by: 71.

A group could only be undone with undo, not later.

## Acceptance

- A sprite placed once, with nothing but its character, that plays once
  through and holds only plain placements, is put back into its clip, and its
  definition goes when nothing else uses it. Grouping then ungrouping gives
  back the original clip. Tested under `ci`, and seen to fail without the
  closing removes and without the stacking check.
- The timeline menu ungroups the sprite on the selected depth and refuses a
  depth that holds no sprite. Tested in `editor_window_tests`, and seen to
  fail with the menu item not wired.
