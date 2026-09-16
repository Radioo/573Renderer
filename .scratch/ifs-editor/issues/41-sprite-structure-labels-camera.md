# Structure, label and camera edits inside sprites

Status: resolved

Blocked by: 40.

Sprites carry their own labels (16783 over IIDX 33) and their own cameras (811,
more than the 647 in roots), so label, camera and structure edits have to name
the clip they apply to rather than assume the root.

## Acceptance

- Adding and removing depths and frames, adding, renaming, moving and removing
  labels, and adding, editing and removing a camera all take the clip they apply
  to and work on a sprite the same way as on the root.
- A clip that no longer exists is refused with a message rather than falling
  back to the root.
- Tested under `ci` on a sprite.
