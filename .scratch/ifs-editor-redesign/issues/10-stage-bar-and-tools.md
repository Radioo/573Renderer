# Stage bar and tool strip

Status: resolved
Blocked by: 01

Breadcrumb, overlay toggles, zoom control, fit and picture saving on the stage bar; Select, Anchor, Pan, Zoom and Motion sketch tools.

Landed: the stage bar (breadcrumb, overlay toggles, the minus/percentage/plus
zoom control with `view.zoom_in` and `view.zoom_out`, Fit, Picture) and the
tool strip (Select, Anchor, Pan, Zoom, Motion sketch) as views over the command
registry, `Editor::Tool` on the viewport with the Anchor, Pan and Zoom
gestures, Space held to pan with a tap still playing, and the Playback menu's
sketch tick replaced by the tool. Covered by three viewport widget tests, the
tool strip and stage bar zoom window tests, and the sketch tests now driving
`tool.sketch`. Documented in docs/editor.md.
