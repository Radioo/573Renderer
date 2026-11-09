# Filter lists that change inside a span

Status: resolved

Blocked by: 57.

Own refused 600 IIDX 33 spans whose updates change the filter list itself,
because a keyframe track had to hold the same count of numbers on every key.
A filter keyframe also could not gain or lose a filter.

## Acceptance

- A stepped track's keyframes may hold different counts of numbers, and every
  other track still may not. Tested under `ci`.
- A span whose filter list changes shape is owned and written back byte for
  byte. Tested under `ci`, and the span survey owns the spans it used to refuse.
- A filter keyframe gains a colour matrix or HSV filter that changes nothing,
  and loses any filter, down to an empty list, which afp-core draws as no filters. Tested under `ci`.
- The inspector offers both from a right-click on a Filters keyframe.
