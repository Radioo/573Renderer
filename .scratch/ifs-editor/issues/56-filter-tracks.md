# Owning a depth whose updates change its filters

Status: resolved

Blocked by: 55.

4737 spans in IIDX 33 change their filter list on later frames, and own refused
them.

## Acceptance

- A filter list is kept in a track as numbers and read back to the same
  filters; numbers that do not follow the layout are refused. Tested under `ci`.
- A span whose updates carry filters is owned with a stepped Filters track and
  detaches byte for byte; a span whose updates leave them alone keeps them on the
  baked create placement; a span whose list changes shape is refused. Tested
  under `ci`.
- A survey measures how the install's spans use filters, and the own survey
  reports what is still refused.
