# Owning without a project and the drift sheet

Status: resolved
Blocked by: 05

Keyframe this depth creates the project at a suggested path when none is open. Drift at project open becomes one review sheet.

Landed: `Depth > Keyframe this depth` makes a project beside the IFS when none
is open (`Window::SuggestedProjectFolder`, `Window::MakeProjectIn`) and the
per-entry drift dialogs became one `Editor::DriftSheet` with a row an entry and
tick-all buttons. Covered by a window test for the first and a widget test for
the sheet. Documented in docs/editor.md.
