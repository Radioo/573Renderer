# Saving the work area as PNG frames

Status: resolved

Blocked by: 78, 79.

A frame could be saved on its own, but a stretch of animation meant saving each
frame by hand.

## Acceptance

- The work area, or the clip, saves as one opaque PNG per frame at the stage
  size, each the same as saving that frame on its own, with progress shown and
  the viewport and host put back afterwards. Tested in the live window tests,
  and seen to fail without the seek, the work area's end, the black background
  or the restore.
- Without a preview it says why. Tested in `editor_window_tests`.
