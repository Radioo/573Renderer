# Keyframe editing on the timeline

Status: resolved

Blocked by: 31, 30.

Milestone 7 built the keyframe model and made export sample it, but nothing in
the editor ever creates or changes a keyframe: `AddKeyframe`, `SetKeyframeValue`,
`SetKeyframeEase`, `RetimeKeyframe` and `RemoveKeyframe` have only tests calling
them. Owning a depth gives a key on every frame and the user cannot touch any of
them, so no animation can actually be authored. This ticket is the consumer the
spec already asked for: "a property of an authored depth is a list of keyframes
with an ease between them, edited on the timeline".

## Acceptance

- Selecting an owned depth shows one timeline row per animated property under
  its depth row, with a mark at every keyframe of that property.
- A keyframe can be selected, and the row and frame it is on are what identify
  it.
- On an existing property a keyframe can be added at a frame, removed, moved to
  another frame, and given an ease of hold, linear or cubic bezier.
- Adding a keyframe on a frame the track already covers does not change what any
  frame samples, for a hold or a linear segment.
- Removing the last keyframe of a property is refused rather than silently
  ending the animation, because the property would stop being written.
- Every edit is one undoable step, runs the reload loop, and writes the project
  manifest, the same as owning and detaching.
- The model edits are tested under `ci`.

## Out of scope

- Animating a property the placement never carried. That needs an identity value
  per property and is its own ticket.
- Dragging a curve; the bezier is entered as its four numbers.
