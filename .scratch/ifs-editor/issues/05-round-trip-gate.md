# Round trip gate over a local game install

Status: resolved

Blocked by: 03, 04.

A `local` test that round-trips every IFS under `R573_IIDX_DIR/data`.

## Acceptance

- Every entry is encoded again through the writers; the test fails on any entry whose decoded bytes differ, naming the IFS and entry.
- It reports the whole-file byte-identity count without failing on it.
- It skips when `R573_IIDX_DIR` is not set, and reports progress per file.

## Comments

2026-09-15: `tests/local/ifs_round_trip_tests.cpp`, ctest label `local`, documented in docs/local_regression.md. Passes on IIDX 33.

2026-09-15, after review: the gate now writes an archive built from re-encoded binary XML and texture entries, compares decoded content and container fields against the original with full entry paths, verifies the manifest MD5 on read back, reports progress per file, and measures packer and tree size reproduction. It was shown to fail on an injected pixel corruption. Passes on all 6146 IIDX 33 archives.
