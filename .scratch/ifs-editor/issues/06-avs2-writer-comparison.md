# Compare writers against avs2-core

Status: resolved

Blocked by: 01, 02.

A `local_dll` test that loads avs2-core from `R573_IIDX_DIR` and compares its binary XML and LZ77 writers with ours on the same inputs.

## Acceptance

- Binary XML: documents written by us, read and written back by avs2-core, compare byte for byte.
- LZ77: game image payloads compressed by both compare byte for byte.
- Skips when the install is not set.

## Comments

2026-09-15: both halves in `tests/local/avs_writer_contract_tests.cpp`. Game documents go through the round trip gate (05) instead of this test, since reading them needs the IFS reader.
