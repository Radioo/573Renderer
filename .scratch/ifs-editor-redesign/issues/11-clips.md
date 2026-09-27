# Clip navigation and in context editing

Status: resolved
Blocked by: 10

The clip box goes; the breadcrumb, double-clicking a sprite bar or its object, and Escape to go up replace it. In context editing dims the root around the open clip.

Landed: the clip combo box is gone (`clips_` + `clip_index_` behind the stage
bar's breadcrumb), a sprite is entered by double-clicking its timeline bar, its
object on stage or its library row, `clip.leave` (Escape) goes back up, and
`clip.in_context` (on by default) keeps the root on screen with everything but
the open clip dimmed, the clip's edge drawn, its depths' outlines mapped onto
the root and drags mapped back through the placement. A clip that is not placed
on the root at the playhead falls back to being shown on its own and says so.
Covered by `Document::OutlineThrough` document tests, a viewport widget test for
the dimming, and window tests for entering, leaving and the mapped drag.
Documented in docs/editor.md.
