# Duplicating an animation

Status: resolved

Blocked by: 83.

An animation could be added from a template, renamed or removed, but not
copied whole, so trying a change on a copy meant rebuilding it.

## Acceptance

- A duplicate copies the data, byte order, listing and shape files under the
  new name and renames its own export, leaving the original as it was; a taken
  or unloadable name and an unlisted animation are refused. Tested under `ci`,
  each part seen to fail the tests when dropped.
- The game's host loads the copy under its new name and draws it, and the
  original, exactly as before. Tested under `local` against `title.ifs`, and
  seen to fail without the shape copy.
- The package menu duplicates and opens the copy. Tested in
  `editor_window_tests`, and seen to fail when the copy is not opened.
