# Duplicating a depth with Ctrl+D

Status: resolved

Duplicating a depth always asked for the depth to copy onto, which is a dialog
for the one answer most wanted: just above.

## Acceptance

- Ctrl+D copies the selected depth's span under the playhead onto the first
  depth above it that is free for it, with no dialog, as one undo step, and
  selects the copy; a depth showing nothing there is reported. Tested in
  `document_tests` and `editor_window_tests`, and seen to fail when a depth is
  skipped, a taken one is used, the report is dropped or the shortcut is
  changed.
