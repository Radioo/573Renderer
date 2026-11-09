# Trimming a clip to the work area

Status: resolved

Cutting an animation down to a stretch of frames meant removing the frames one
at a time, which dropped the creates of every span that began before the
stretch and broke what the kept frames showed.

## Acceptance

- `Edit > Trim the clip to the work area` (Ctrl+Shift+X) keeps only the work
  area's frames, and every kept frame shows what it showed, as one undo step
  that clears the work area and keeps the playhead on the same content. It says
  how many spans crossing the new first frame start again (sprites and clips),
  and is refused without a work area, for the whole clip, and while the project
  owns a depth in the clip. Tested in `document_tests` and
  `editor_window_tests` (live on `title.ifs` too), and seen to fail without the
  range checks, the removal of spans outside, the restart rule, the frame
  removal at either end, the work-area and ownership checks, the clearing, the
  seek, the quiet case and the shortcut. The live test caught the playhead
  being worked out after the edit had already clamped it.
