# Onion skin

Status: resolved

Blocked by: 21, 78.

A pose could only be compared with its neighbours by stepping back and forth.

## Acceptance

- The viewport draws ghost pictures faintly over the frame, and a new frame
  clears them. Tested in `editor_widget_tests`, and seen to fail without the
  painting or the clearing.
- With onion skin on, the previous and next frames are shown while stopped and
  the host is left on the frame shown; turning it off restores the picture.
  Tested in the live window tests, and seen to fail without the ghost renders or
  the seek back.
