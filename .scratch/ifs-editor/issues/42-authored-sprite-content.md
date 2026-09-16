# Authored content inside sprites

Status: ready-for-agent

Blocked by: 41.

Owning a depth, its keyframes, its script and its export all assume the root.
Most animated content is in sprites, so authoring has to reach them too.

## Acceptance

- An authored depth records which clip it belongs to, and own, detach,
  keyframe edits, script edits and export all work in that clip.
- The project manifest records the clip only for a sprite, so a manifest
  written before this change still reads, as owning root depths.
- Owning a sprite depth and detaching it again with no edit gives back the clip
  unchanged, the same promise own makes for the root.
- Tested under `ci`, and the `local` own and detach survey covers sprite spans.
