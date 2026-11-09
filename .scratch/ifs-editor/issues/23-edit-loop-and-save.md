# The edit loop and saving

Status: resolved

Blocked by: 22.

The loop every later ticket in this milestone rides on: change the model,
encode, reload, seek back, and eventually write the file.

## Acceptance

- A `Document` holds the archive, the outline and whether it has unsaved
  changes, and hands out the encoded IFS bytes for the preview host.
- Writing an animation back into its archive re-encodes the animation and its
  byte order script with `AfpAnimation::WriteStored` and replaces both entries.
- After a change the window encodes the document, reloads the package in the
  host under the same name, seeks to the frame the timeline is on and renders,
  so the viewport shows the edit without losing the playhead.
- `File > Save` writes over the opened path with `Ifs::Write`; `File > Save
  as...` asks for a path and remembers it. The window title marks an unsaved
  document and closing one asks first.
- The encode and write-back are Qt-free and tested under `ci`: an animation
  changed in the model comes back changed after a re-read, and every other
  entry of the archive is untouched.
- A `local_dll` test changes a placement in a shipped package, runs the loop
  against a real host and checks afp-core still reports the frame count and
  labels the model has.
