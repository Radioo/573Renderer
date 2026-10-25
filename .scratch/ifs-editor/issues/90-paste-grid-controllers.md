# Grid controllers in a paste across animations

Status: resolved

Blocked by: 84, 86.

Tracing what reaches a definition by id (ticket 86) found that a placement's
grid controller names one too. A paste into another animation followed and
renumbered only the character, so a pasted grid controller kept the source's
id, which in the target named nothing or the wrong definition.

## Acceptance

- Grid controllers on a pasted placement and inside a pasted sprite name
  definitions the paste brought along under new ids. Tested under `ci`; the
  test failed before the fix, and dropping the grid id from any of the three
  places it is followed fails it again.
