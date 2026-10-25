# Visibility and lock switches in the timeline gutter

Status: resolved

Blocked by: 74, 76.

Hiding or locking a depth took the timeline menu, and only a lock was marked,
as a letter after the depth number.

## Acceptance

- The gutter draws an eye and a padlock per depth that show whether it is
  hidden and locked, and clicking them toggles it without choosing the depth or
  a frame. Tested in `editor_widget_tests`, and seen to fail without the press
  or the drawing.
- The window hides, locks and shows a depth through the switches. Tested in
  `editor_window_tests`, and seen to fail without either connection.
