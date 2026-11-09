# Editing inside sprites

Status: resolved

Blocked by: 38.

Every edit, the timeline and the inspector address the root clip only. Measured
over IIDX 33 (`afp_clip_nesting_survey_tests`), that is the smaller part of the
content:

- 29110 animations, every one with more placements inside its sprites than in
  its root.
- 16384205 placements inside sprites against 7339644 in roots, so 69% of all
  placements are out of the editor's reach.
- 171786 sprites, 54598 of them with a timeline of their own.
- Sprites are always defined in the root and never inside another sprite, so a
  clip is the root or one sprite, never a path.
- 111157 of the sprites carry an export name.

## Acceptance

- A clip of an animation is named by a `ClipId`: the root, or a sprite by its
  character id. The document lists an animation's clips, the root first, with
  each sprite's export name when it has one.
- Picking an animation offers its clips, and picking a clip shows that clip's
  depths and labels on the timeline and its placements in the inspector.
- Placement fields and library call arguments of a sprite are edited the same
  way as the root's, through the same undo and reload loop.
- The viewport keeps showing the root animation, and says so, because the host
  cannot yet render a sprite on its own (ticket 43). Playback plays the root and
  is offered only while the root is the selected clip.
- The clip model and the inspector over a sprite are tested under `ci`.
