# The work area

Status: resolved

Blocked by: 39, 77.

Playback always ran over the whole clip.

## Acceptance

- B and N set the start and end of a work area at the playhead, the ruler
  shades it, it can be cleared, and playback loops inside it. Tested under
  `ci` (`playback_tests`, `editor_widget_tests`, `editor_window_tests`) and
  with the host against `title.ifs`, where playback that ignored it was seen
  to fail.
