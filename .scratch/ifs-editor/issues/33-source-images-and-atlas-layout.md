# Source images and atlas layout

Status: ready-for-agent

Blocked by: 32.

## Acceptance

- A project keeps a copy of every source image it owns, and the IFS entry is
  produced from that copy at export.
- Atlas layout for the images a project owns is computed at export; atlases
  that came from baked data keep the layout they had.
- The layout is deterministic and the texture list it produces matches the
  shape the shipped data uses, measured rather than assumed.
- Tested under `ci` over the model.
