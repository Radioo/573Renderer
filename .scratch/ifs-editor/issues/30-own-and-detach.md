# Own and detach

Status: resolved

Blocked by: 29, 31.

## Acceptance

- Owning a depth over a frame range turns its baked placements into authored
  content, keying every frame each property is actually set on.
- Detaching turns authored content back into baked data and drops its source.
- Owning a depth and detaching it with no edit in between produces the
  placements that were there before, byte for byte, measured over every span in
  the target build's install rather than on a sample.
- The baked data of an authored depth is read-only in the inspector.
- Tested under `ci` over the model.
