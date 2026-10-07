# Dropping characters on the stage

Status: resolved

Blocked by: 92.

Placing a character meant a menu, two dialogs and then dragging it into place.

## Acceptance

- A character is placed on a new depth with its origin at a stage point, or not
  at all. Tested under `ci`, and seen to fail without the move.
- The viewport takes a drop of the library's character format, reports the
  stage point, and ignores other data. Tested in `editor_widget_tests`, and
  seen to fail without the signal.
- The library's drag data carries the character, and a drop places it on the
  next free depth at the point; a drop while a sprite is edited over the root
  view is refused. Tested in `editor_window_tests`, and seen to fail when the
  drop is not connected or the refusal is dropped.
