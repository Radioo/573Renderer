# IFS container reader and writer

Status: resolved

Blocked by: 02, and the imagefs container format being documented from avs2-core.

Read an IFS into a model of header fields, the directory tree and entry bytes, and write the model back.

## Acceptance

- Header, manifest and data layout follow avs2-core's imagefs driver, including the manifest MD5 when its flag is set.
- Entry names keep their stored form; the tree keeps its order.
- An unmodified IFS read and written again is byte-identical when the layout rules are fully known; any rule that cannot be reproduced is reported, not guessed.
- Files that are not IFS files (such as the 256-byte stubs in IIDX 33) return an error.

## Comments

2026-09-15: `src/formats/ifs_archive.h`, `ifs_read.cpp`, `ifs_write.cpp`, `ifs_layout.{h,cpp}`; unit tests in `tests/formats/ifs_archive_tests.cpp`. The round trip gate reproduces 6145 of 6146 IIDX 33 archives byte for byte; the exception is a shipped file truncated before its data offset.
