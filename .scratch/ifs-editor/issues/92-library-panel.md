# A library panel

Status: resolved

Blocked by: 19, 86.

Characters could only be reached through the add depth dialog, with no way to
see what an animation defines or which of it is used.

## Acceptance

- Uses are counted for every placement naming a character, in the root and in
  every sprite, through its character and its grid controller. Tested under
  `ci`, and seen to fail without the sprites or the grid controllers.
- A library panel lists the animation's characters with their uses, places one
  on a new depth from the playhead, and shows a sprite on its own when it is
  double-clicked. Tested in `editor_window_tests`, and seen to fail without the
  refill, the double-click or the placement.
- A closed panel comes back from `View > Panels`, and a layout saved before the
  library falls back to the default arrangement. The menu is tested in
  `editor_window_tests`, and seen to fail without it.
