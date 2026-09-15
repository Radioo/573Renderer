# IFS container reader and writer

Status: ready-for-agent

Blocked by: 02, and the imagefs container format being documented from avs2-core.

Read an IFS into a model of header fields, the directory tree and entry bytes, and write the model back.

## Acceptance

- Header, manifest and data layout follow avs2-core's imagefs driver, including the manifest MD5 when its flag is set.
- Entry names keep their stored form; the tree keeps its order.
- An unmodified IFS read and written again is byte-identical when the layout rules are fully known; any rule that cannot be reproduced is reported, not guessed.
- Files that are not IFS files (such as the 256-byte stubs in IIDX 33) return an error.
