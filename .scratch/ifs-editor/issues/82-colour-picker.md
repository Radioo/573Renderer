# Picking a colour

Status: resolved

Blocked by: 24, 37.

Colours could only be typed as four numbers.

## Acceptance

- Right-clicking an editable colour row picks a colour into it, and other
  rows do not offer it. Tested under `ci` (`colour_pick_tests`,
  `editor_window_tests`), and seen to fail with every row offering it.
- The channel order is read from afp-core, not assumed: r, g, b, a for both
  the unpacked and packed forms.
