# Editing a bezier ease as a curve

Status: resolved

Blocked by: 53.

A bezier ease was typed as four numbers with no picture of the curve, and could
only be set on one keyframe at a time.

## Acceptance

- The document exposes the ease function sampling uses, keeps dragged control
  points inside the segment's time, names a set of preset curves, and sets one
  ease on several keyframes at once, all or nothing. Tested under `ci`.
- The bezier choice opens a dialog that draws the curve, lets both handles be
  dragged, offers the presets and keeps the numbers editable.
- An ease chosen on a selected keyframe applies to the whole selection.
