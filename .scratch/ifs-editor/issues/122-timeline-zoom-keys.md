# Timeline zoom keys

Status: resolved

The timeline could only be zoomed with Ctrl and the wheel, and zooming in then
out did not always come back to fitting the panel.

## Acceptance

- `=` and `-` zoom the timeline in and out around the playhead, and every zoom
  step starts from the stored zoom or the fitting scale, so in then out returns
  to fitting. A zoom on an empty timeline does nothing. Tested in
  `editor_widget_tests` (inside a scroll area, keys and wheel) and
  `editor_window_tests` (600 frames, which failed on the old read-back), and
  seen to fail without the empty guard, either direction, the stored scale,
  the rounding allowance, the scroll or either connection.
