# Arranging depths in the stacking order

Status: resolved

Changing which depth draws over which took a move to a free depth number and
back again, and a move could not swap two depths at all.

## Acceptance

- `Edit > Arrange` brings the selected depth forward or to the front, or sends
  it backward or to the back, among the depths shown at the playhead, with
  After Effects' shortcuts, as one undo step that carries project-owned records
  to their new depths. It is refused, leaving the clip alone, when a span would
  run into another span, when a mask is involved, or when the depth is already
  at the end. Tested in `document_tests` and `editor_window_tests`, and seen to
  fail when every direction passes all depths, when the depth below is not
  reversed, when depths not shown on the frame count, without either mask
  check, with a scratch depth that is in use, with the changes misreported, and
  in the window without moving the owned records, without saving, without
  following the selection, or with `Bring to front` wired to `Forward`.
