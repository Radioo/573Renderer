# Zooming the timeline

Status: resolved

Blocked by: 36.

The timeline squeezed every frame of an animation into the panel's width, so a
long animation gave each frame a pixel or two and keyframes were hard to hit.
The ruler showed only the current frame number.

## Acceptance

- Ctrl and the mouse wheel zoom the timeline in and out around the frame under
  the cursor, which stays under the cursor; zooming out past the panel width
  goes back to fitting the whole animation.
- The zoom survives reloads after an edit and switching clips.
- The ruler marks frames at a spacing that stays readable at any zoom.
- While zoomed, scrubbing and playback keep the playhead in view.
