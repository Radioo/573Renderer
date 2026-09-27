# Go to frame

Status: resolved

Reaching a far frame meant scrubbing the ruler or stepping; there was no way to
type the frame.

## Acceptance

- `Playback > Go to frame...` (Alt+Shift+J) asks for a frame of the clip,
  suggesting the playhead, and seeks there; with nothing open it asks nothing.
  Tested in `editor_window_tests`, and seen to fail without the empty-clip
  check, the answer or the shortcut.
