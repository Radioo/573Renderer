# Motion sketch

Status: resolved

A hand-drawn path meant keying the translation frame by frame; After Effects'
Motion Sketch records a drag while the composition plays.

## Acceptance

- With `Playback > Motion sketch while dragging` on, dragging an owned depth on
  the stage plays the animation and records the pointer's offset on every
  frame shown until release; the release stops playback, returns to the start
  frame and keys each recorded frame's translation as the start frame's plus
  that offset, replacing keyframes in that range, as one undo step. Tested in
  `document_tests`, `editor_window_tests` and, with real playback, the live
  window tests.
- Baked depths move as usual; with the mode off, owned depths move as usual.
- An empty sketch, a frame outside the owned range and a 3D depth are refused.
