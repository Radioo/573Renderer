# A history panel

Status: resolved

Blocked by: 25.

Undo and redo went one step at a time, with only the next step's name in the
Edit menu.

## Acceptance

- The history lists its steps in order and jumps to any of them, back or
  forward, in one call; a position past the end is refused. Tested under `ci`,
  and seen to fail without the redo walk or the redo names.
- A history panel lists the steps, greys the ones that redo would bring back,
  follows undo, and jumps when a row is clicked. Tested in
  `editor_window_tests`, and seen to fail without the refill, the greying or
  the jump.
