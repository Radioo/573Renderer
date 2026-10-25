# Rulers and guides on the stage

Status: resolved

Blocked by: 88.

Lining things up meant snapping to the stage or another object; there was no
way to mark a line of your own.

## Acceptance

- A move snaps to guides on its own axis as well as to the stage and other
  objects. Tested under `ci`, and seen to fail without the guides or with the
  axes swapped.
- With rulers on, a guide is pulled from a ruler, drawn, snapped to, moved,
  dropped back to remove it, and cleared from the View menu; with rulers off a
  press there picks as before. Tested in `editor_widget_tests`, and seen to fail
  without the ruler press, the rulers setting, the removal or the clear.
