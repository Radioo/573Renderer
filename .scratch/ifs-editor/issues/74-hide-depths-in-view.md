# Hiding depths in the view

Status: resolved

Blocked by: 21, 22.

There was no way to look past a depth without removing it from the document.

## Acceptance

- A depth hidden in the view is left out of what the host renders, cannot be
  picked on stage, shows grey bars, and the document and undo history do not
  change. Tested under `ci` (`hidden_depths_tests`, `editor_widget_tests`,
  `editor_window_tests`) and against IIDX 33's `title.ifs` with the host, where
  switching the filter off was seen to fail.
