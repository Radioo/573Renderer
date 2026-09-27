# Owning depths whose updates change curves

Status: resolved

Blocked by: 61.

Own refused the 269 spans in IIDX 33 whose updates carry deformation curves,
because whether a later curve set replaces the whole deformation had not been
read.

## Acceptance

- Curves are a stepped track held until updated, keyed with each update's own
  curve set, and own and detach give the clip back unchanged. Tested under `ci`.
- A curve key that would overrun the controller the first set allocated is
  refused. Tested under `ci`, and seen to fail without the check.
- A trim that moves a span's start later folds curve sets per slot, and is
  refused when a kept update would not fit the folded set. Tested under `ci`,
  and the refusal seen to fail without the check. A trimmed shipped curve span
  draws its kept frames identically, tested under `local` against
  `led_effects.ifs`, and seen to fail with the fold skipped.
- Updates with and without an extended word in one span own and detach
  exactly, and a curve key moved onto a frame gets the word. Tested under `ci`.
- Over the install every span owns and detaches identically (826810 of
  826810). Checked under
  `local` with `placement_span_survey_tests`.
