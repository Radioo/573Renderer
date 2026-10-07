# Selecting, copying and moving several keyframes

Status: resolved

Blocked by: 36.

Keyframes could only be picked and moved one at a time, and could not be copied.

## Acceptance

- The document copies selected keyframes with their spacing, pastes them at a
  frame (replacing keyframes already there and starting missing tracks),
  removes them and moves them together, each change all or nothing. Tested
  under `ci`.
- The timeline selects keyframes by click, Ctrl-click and box, drags the
  selection as one undo step, and offers copy, paste, delete and select all
  from the keyboard and the lane menu.
- The single-keyframe move is replaced by the selection move.
