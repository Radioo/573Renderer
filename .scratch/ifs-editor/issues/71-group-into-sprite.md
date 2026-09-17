# Grouping depths into a sprite

Status: resolved

Blocked by: 40, 59.

The editor could not do what After Effects calls pre-compose: turn a set of
layers into a nested composition.

## Acceptance

- A range of depths and frames moves into a new sprite placed where they were,
  with removes and end frames kept in order, and ranges that cannot be moved
  without changing what scripts, names, 3D or clip depths see are refused.
  Tested under `ci`.
- A grouped shipped range draws the same frames as before, both from frame 0
  and from a later frame. Tested under `local` against `led_effects.ifs`, and
  seen to fail with the sprite's frames shifted.
- The timeline menu groups the selected depth and the ones above it, and the
  new sprite shows in the clip box. Tested in `editor_window_tests`, and seen
  to fail without the refill.
