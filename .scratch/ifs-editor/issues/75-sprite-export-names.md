# Naming a sprite's export

Status: resolved

Blocked by: 40, 65.

A sprite the editor made, or any other, could not be given the name the game
and other animations look symbols up by.

## Acceptance

- A sprite's export is added, renamed and removed in the order the game's
  lookup needs, and names that clash, belong to the animation itself or its
  helpers, or are imported by another animation in the file are refused.
  Tested under `ci`, and the import rule seen to fail without its check.
- With a sprite picked, the timeline menu names its export and the clip box
  follows. Tested in `editor_window_tests`, and seen to fail without the
  refill.
