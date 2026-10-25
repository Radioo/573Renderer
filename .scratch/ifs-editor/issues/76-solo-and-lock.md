# Solo and lock

Status: resolved

Blocked by: 74.

After Effects lets a layer be shown alone or kept from being grabbed on
stage; the editor had only hide.

## Acceptance

- Solo hides every other depth of the clip in the view, and lock keeps a depth
  from being picked or dragged on stage and marks it in the gutter, without
  changing the document. Tested in `editor_window_tests` and
  `editor_widget_tests`, and both seen to fail with their filter switched off.
