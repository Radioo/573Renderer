# Shapes in the round trip gate

Status: resolved

Blocked by: 10.

## Acceptance

- Every `geo/` entry of a package with a `magic` file is read, written and placed into the re-encoded archive.
- The gate fails on any difference in the decoded shape, naming the archive and entry.
- It reports how many shapes come out byte-identical.

## Comments

2026-09-15: All 273660 IIDX 33 shapes decode equal and come out byte-identical. An injected vertex bit flip failed all 2013 shapes on the sample set.
