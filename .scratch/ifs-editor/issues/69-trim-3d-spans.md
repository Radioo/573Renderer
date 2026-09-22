# Trimming 3D spans

Status: resolved

Blocked by: 60.

A trim refused every span with a 3D placement, although IIDX 33 has hundreds of
thousands of 3D updates.

## Acceptance

- A 3D span's start moves later by folding translation, `tz` and 3x3 updates
  the way the parser applies them in 3D. Spans that switch between 2D and 3D
  stay refused. Tested under `ci`.
- A trimmed shipped 3D span draws its kept frames identically. Tested under
  `local` against `arena.ifs`, and seen to fail with the 3D fold skipped.
