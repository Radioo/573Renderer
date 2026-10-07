# Removing unused definitions

Status: resolved

Blocked by: 84.

Removing depths, grouping and pasting leave sprite and shape definitions that
nothing places any more, together with their shape files.

## Acceptance

- Sprites and shapes nothing places, exports or names through a grid controller
  are removed with their shape files and listing; images are kept; an
  animation whose sprites define sprites is refused. Tested under `ci`, each
  rule seen to fail the tests when dropped.
- Which afp-core paths reach a definition by id is traced and written down
  before anything is removed.
- The package menu removes them as one undo step, and says when nothing is
  unused. Tested in `editor_window_tests`, seen to fail when the result is not
  applied.
