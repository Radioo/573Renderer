# Start screen

Status: resolved
Blocked by: 02

Recent files with thumbnails and project badges, open buttons, drop target, the game install card.

Landed: the start screen in the centre stack with Open IFS, Open project and
New project, the recent files (name, folder, animation count, project badge),
the game install card and the no-install note, plus dropping an IFS or a
project folder anywhere in the window. Thumbnails of the packages are not
drawn: an animation's picture comes from the preview host, which is not running
while the start screen is up. Covered by a window test. Documented in
docs/editor.md.
