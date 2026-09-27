# Editing an authored value in the inspector

Status: resolved

Blocked by: 36.

An owned depth's placement is shown in the inspector with every cell read-only,
because its baked data is produced from the keyframes. That leaves no way to
change what a keyframe holds: the value can only be the one own captured.

## Acceptance

- With a keyframe selected, the inspector shows that keyframe's value in the
  same cells the placement fields use, and committing a cell sets the keyframe's
  value rather than the placement's.
- The inspector says which frame the value belongs to, so it is never mistaken
  for the placement at the playhead.
- A property of an owned depth that no keyframe holds stays read-only.
- Committing a value runs the same undo, reload and manifest steps as every
  other edit.
- The model side is tested under `ci`.
