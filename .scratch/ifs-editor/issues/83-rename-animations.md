# Renaming an animation

Status: resolved

Blocked by: 62.

An animation could be added or removed but not renamed.

## Acceptance

- A rename moves the data, byte order, listing, shapes, name and own export,
  and is refused for a bad or taken name, an unlisted animation and an
  imported one. Tested under `ci`, and seen to fail with the shapes left
  behind.
- The package menu renames and keeps the animation open. Tested in
  `editor_window_tests`, which first failed because the reload read the old
  path.
- The game's host loads a renamed shipped animation under its new name and
  draws it the same. Tested under `local` against `title.ifs`.
