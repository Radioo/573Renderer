# Own and detach

Status: ready-for-agent

Blocked by: 29, 31.

## Acceptance

- Owning a depth over a frame range turns its baked placements into authored
  content with a keyframe on every frame and every property the placements
  carry.
- Detaching turns authored content back into baked data and drops its source.
- Owning a depth and exporting it with no edit in between produces the
  placements that were there before, byte for byte.
- The baked data of an authored depth is read-only in the inspector.
- Tested under `ci` over the model.
