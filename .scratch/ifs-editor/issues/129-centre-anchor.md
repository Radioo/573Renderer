# Centre the anchor in the content

Status: resolved

The origin could only be typed into the inspector, which moves the object on
screen; there was no way to move the point an object turns and scales about
while keeping it where it is, After Effects' Center Anchor Point in Layer
Content.

## Acceptance

- `Edit > Centre the anchor in the content` (Ctrl+Alt+Home) moves the chosen
  depth's origin to the centre of its content on the playhead's frame, over
  its whole span, shifting every matrix placement's translation by the shift
  carried through that placement's own matrix, as one undo step. Tested in
  `document_tests` against the stage outlines of every frame.
- The game draws the result as before, to the format's precision of a
  twentieth of a pixel: `local_dll` renders a scaled span of IIDX 33's
  `title.ifs` through the game's DLLs before and after, with a one pixel
  control proving the allowance cannot hide a misplacement.
- Refused, changing nothing: no content with a known box, 3D, geometry, an
  anchor already at the centre, and a depth the project owns. Tested in
  `editor_window_tests`.
