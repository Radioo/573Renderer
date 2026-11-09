# Trimming a span's start and end

Status: resolved

Blocked by: 59.

A depth's bar could be moved but not made shorter or longer.

## Acceptance

- The document trims a span's end both ways, moves its start earlier into free
  frames, and moves its start later by folding the skipped updates into the
  first placement the way the placement parser applies them, refusing any trim
  that would change what the kept frames show. Tested under `ci`, and the check
  was seen to refuse a fold with the colour step removed.
- An owned span's keyframes are cut to the new range with held values at the
  new ends, and the span is rewritten from them. Tested under `ci`.
- Dragging a bar's edge on the timeline trims it; the widget tests cover both
  edges.
