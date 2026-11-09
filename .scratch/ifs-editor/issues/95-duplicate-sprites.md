# Duplicating a sprite and swapping a depth to it

Status: resolved

Blocked by: 92.

A variant of a sprite meant rebuilding it, and pointing a depth at another
character meant typing its id into the inspector.

## Acceptance

- A duplicated sprite is a copy of the timeline under the next free id, defined
  on the original's frame; a non-sprite is refused. Tested under `ci`, and seen
  to fail with the old id or the wrong frame.
- The library duplicates a sprite and puts a character on the selected depth.
  Tested in `editor_window_tests`, and seen to fail without the clip refill or
  the swap.
- The host draws a depth pointed at a duplicate exactly as it drew the
  original. Tested under `local` against `title.ifs`, and seen to fail with the
  copy emptied.
