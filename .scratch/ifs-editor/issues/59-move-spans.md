# Moving a span in time and to another depth

Status: resolved

Blocked by: 51.

A depth's bar could not be moved along the timeline, and a depth could not be
moved in the drawing order.

## Acceptance

- The document moves a span by whole frames with its end frames and remove,
  keeping a remove ahead of a create in the same frame, and refuses moves that
  leave the clip or collide. Tested under `ci`, including the remove of the span
  before it.
- The document moves a span to a free depth number and refuses a busy or
  reserved one. Tested under `ci`.
- An owned span's range and keyframes follow the move. Tested under `ci`.
- Dragging a bar on the timeline moves the span; the menu moves it to another
  depth. The drag is covered by the widget tests.
