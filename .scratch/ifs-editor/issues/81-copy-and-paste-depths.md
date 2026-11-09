# Copying depths between clips

Status: resolved

Blocked by: 40, 66.

A span could be duplicated inside its own clip but not carried into another.

## Acceptance

- A depth's span is copied and pasted at a frame onto a free depth of any clip
  in the same animation; pastes that would not fit, overlap, cross animations
  or make a sprite place itself are refused. Tested under `ci`, and the
  self-placement check seen to fail without it.
- The timeline menu copies and pastes. Tested in `editor_window_tests`, and
  seen to fail with the paste skipped.
