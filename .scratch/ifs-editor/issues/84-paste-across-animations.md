# Pasting depths into another animation

Status: resolved

Blocked by: 81.

A copied depth could only be pasted inside the animation it came from, since a
placement names its character by an id that is local to one animation.

## Acceptance

- A depth pasted into another animation of the package brings the sprites,
  shapes and shape files it reaches along under new ids, with its strings, and
  is refused when it needs an imported or undefined character. Tested under
  `ci`, and each carry step seen to fail the tests when left out.
- The timeline menu pastes across animations and saves a package that defines
  the pasted shape. Tested in `editor_window_tests`, and seen to fail with the
  plain copy. Opening a package drops the copy, seen to fail without the reset.
