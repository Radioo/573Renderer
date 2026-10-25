# Window tests, and editing without a game install

Status: resolved

Blocked by: 58.

The window's own actions (the package and inspector menus, setting edits, the
Playback options) had no tests, and choosing an animation did nothing but show a
message when no game install was set, so an IFS could not be edited at all
without one.

## Acceptance

- Choosing an animation without a host fills the timeline and the inspector from
  the document, and edits refresh them the same way.
- A window test executable drives the real `Editor::Window` with no host and
  isolated settings: opening, editing and undoing a setting, adding and removing
  an animation through the package menu, a refused name, and the background
  option. It runs in `tools/checks.sh`, and was seen to fail with the old early
  return put back.
