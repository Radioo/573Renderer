# Zooming and panning the stage

Status: resolved

Blocked by: 21.

The viewport always fitted the whole stage, so a small layer could not be
picked or dragged with any precision.

## Acceptance

- Ctrl and the wheel zoom around the cursor, the middle button pans, and fitting
  puts both back; picking and dragging follow. Tested in
  `editor_widget_tests`, and seen to fail without the cursor anchor, the pan,
  or the reset.
- A zoom asks the host for a render up to the stage's own size and no larger.
  Tested in `editor_widget_tests`, and seen to fail without the cap.
- The editor build no longer fails now and then at the `ifs_editor` link: the
  plugin deploy of parallel targets deleted each other's temporary copy. Seen
  to fail one relink in three before, and eight in a row passed after.
