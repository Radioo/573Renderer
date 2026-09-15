# Round trip gate over a local game install

Status: ready-for-agent

Blocked by: 03, 04.

A `local` test that round-trips every IFS under `R573_IIDX_DIR/data`.

## Acceptance

- Every entry is encoded again through the writers; the test fails on any entry whose decoded bytes differ, naming the IFS and entry.
- It reports the whole-file byte-identity count without failing on it.
- It skips when `R573_IIDX_DIR` is not set, and reports progress per file.
