# Undo restores authored content too

Status: resolved

Blocked by: 36.

Undo only put the IFS back. The project's owned depths, their keyframes and
their scripts stayed as they were, so undoing a keyframe edit reverted the
viewport but left the keyframe changed, and the next export wrote the undone
edit back into the package. Owning a depth and editing a script were not
undoable at all, and a script edit on a sprite depth also rewrote any root
depth with the same number and first frame.

## Acceptance

- An undo step holds the IFS and the authored content together, and undo and
  redo restore both and write the manifest.
- Owning a depth and editing a script are undoable steps.
- A script edit changes exactly the owned depth it was made on.
- Tested under `ci` on the history model.
