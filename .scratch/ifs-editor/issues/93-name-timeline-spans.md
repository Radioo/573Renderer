# Naming what each timeline span places

Status: resolved

Blocked by: 22, 92.

The timeline showed depth numbers and bars, so telling depths apart meant
selecting each one and reading the inspector.

## Acceptance

- Each span records the character its placement names. Tested under `ci`, and
  seen to fail without it.
- The bar draws that character's name when it is wide enough, and hovering it
  names it. Tested in `editor_widget_tests`, and seen to fail without the
  drawing.
- The window hands the names over, so an open animation's bars are named.
  Tested in `editor_window_tests`, and seen to fail without the hand-over.
