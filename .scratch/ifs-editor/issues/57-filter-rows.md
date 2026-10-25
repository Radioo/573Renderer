# Editing filters as rows

Status: resolved

Blocked by: 56.

A Filters keyframe showed as one long line of numbers, and a baked placement's
filters showed only as "Unknown data".

## Acceptance

- A filter list reads as a row per filter plus a row per colour matrix row and
  its HSV, and those value rows are editable from text; an edit that does not
  fit changes nothing. Tested under `ci`.
- A baked placement shows and edits its filters through its fields.
- A selected Filters keyframe shows the rows instead of its raw value, and an
  edit to one is stored back into the keyframe. Tested under `ci`.
