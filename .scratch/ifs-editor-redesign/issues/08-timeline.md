# Timeline redesign

Status: resolved
Blocked by: 01

Header with transport, frame field, time, label, work area, Timeline and Graph switch, zoom slider. Rows front depth first with eye, lock and solo switches and column header toggles, type icon, character name, keyed badge, bars coloured by character type, change ticks. Scripts lane with a camera button. Work area band buttons. Double-click the ruler to add a label, a flag to rename it. Drag a row to another depth. + Depth picker.

## Acceptance

- The existing drags, trims, snapping, label drags and bracket keys keep working.
- Graph is a mode of the timeline panel, not a dock.

## Landed

- The timeline bar: transport, frame field, time, label, work area with its
  buttons, the Timeline and Graph switch, the zoom commands.
- Rows front depth first.
- Bars coloured by character kind, a tick on every frame a placement changes the
  depth, an amber edge on a depth the project owns.

- Column header toggles in the ruler's gutter: the eye runs `depth.show_all`,
  the padlock runs the new `depth.unlock_all`.
- The scripts lane (`Document::FrameNotes`, `Timeline::DrawNotes`) with its
  Camera button running `clip.camera`.
- Double-click the ruler to add a label, double-click a flag to rename it.
- The + Depth button running the new `depth.add`.
- The zoom slider on the bar, in pixels a frame.

All covered by widget and window tests; documented in docs/editor.md.
