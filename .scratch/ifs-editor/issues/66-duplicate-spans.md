# Duplicating a span onto another depth

Status: resolved

Blocked by: 59.

A depth's span could be moved to another depth but not copied.

## Acceptance

- The document copies a span's placements and closing remove onto a free depth,
  and the copy replays like the original. Busy, reserved and same depths are
  refused without changing anything. Tested under `ci`.
- The timeline menu offers it and selects the copy. Covered by a window test,
  including a refused target.
- Window tests can no longer hang the gate: waits are bounded and a stuck dialog
  is closed and reported.
